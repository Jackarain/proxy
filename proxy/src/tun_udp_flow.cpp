//
// tun_udp_flow.cpp
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/tun_server.hpp"
#include "proxy/proxy_websocket_client.hpp"
#include "proxy/dns_server.hpp"
#include "proxy/http_proxy_client.hpp"
#include "proxy/logging.hpp"
#include "proxy/proxy_util.hpp"
#include "proxy/socks_client.hpp"
#include "proxy/ssl_connect.hpp"
#include "proxy/network_order.hpp"
#include "proxy/wintun_install.hpp"
#include "tun_server_detail.hpp"

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/functional/hash.hpp>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <future>
#include <string>
#include <vector>

namespace proxy {

#if defined(__linux__) || defined(__APPLE__) || defined(_WIN32)

	//////////////////////////////////////////////////////////////////////////
	// tun_udp_flow

	tun_udp_flow::tun_udp_flow(net::any_io_executor executor,
		const std::shared_ptr<tun_server>& owner,
		const proxy_server_option& opt,
		tunio::tun_udp_socket socket)
		: m_executor(std::move(executor))
		, m_owner(owner)
		, m_option(opt)
		, m_conn_id(owner ? owner->next_conn_id() : 0)
		, m_client(socket.client_endpoint())
		, m_started(std::chrono::steady_clock::now())
		, m_proto(static_cast<uint8_t>(tun_conn_proto::udp))
		, m_client_socket(std::move(socket))
	{
		m_expire.emplace(m_executor);

		// 引擎侧会话超时留出宽限，空闲回收由 m_expire 计时器统一负责.
		m_client_socket.set_timeout(std::chrono::seconds(
			tun_detail::udp_idle_timeout(m_option) + 30));
	}

	tun_udp_flow::~tun_udp_flow()
	{
		close();
	}

	void tun_udp_flow::start()
	{
		auto self = shared_from_this();

		net::co_spawn(m_executor,
			[this, self]() -> net::awaitable<void>
			{
				return do_open();
			}, net::detached);
	}

	net::awaitable<void> tun_udp_flow::do_open()
	{
		auto self = shared_from_this();

		// 接收首数据报，确定会话目标并注册.
		{
			char buf[65535];
			net::ip::udp::endpoint sender;
			boost::system::error_code ec;
			size_t n = co_await m_client_socket.async_receive_from(
				net::buffer(buf), sender, net_awaitable[ec]);
			if (ec)
			{
				close();
				co_return;
			}

			m_target = sender;
			m_orig_target = sender;
			m_key = tcp_flow_key{
				m_client.address(), m_client.port(),
				m_target.address(), m_target.port()
			};

			// DNS 查询报文解析查询域名，用于按 proxy_domains_ 分流.
			if (m_target.port() == 53 && n > 0)
			{
				std::string query(buf, n);
				uint16_t qtype = 0;
				dns_parse_query(query, m_dns_qname, qtype);
			}

			if (m_owner)
				m_owner->register_udp_flow(m_key, self);

			// DNS 查询流（目标 53 端口）为内部基础设施流量，不纳入累计连接统计.
			if (m_target.port() != 53 && m_owner)
				m_owner->on_conn_created();

			// 先分流再处理首数据报：DNS 缓存命中可立即回包，无需等待后端.
			if (!resolve_proxy_mode())
			{
				close();
				co_return;
			}

			co_await handle_datagram(buf, n);
			if (m_closed)
				co_return;
		}

		// UDP 后端建立总超时: 代理 ASSOCIATE/CONNECT-UDP 响应不来时
		// 读响应协程会一直挂起 (黑洞/代理假活), 超时关闭会话使在途
		// 操作失败, 避免停止后残留挂起点.
		net::steady_timer conn_timer(m_executor);
		conn_timer.expires_after(std::chrono::seconds(15));
		conn_timer.async_wait(
			[self](const boost::system::error_code& tec)
			{
				if (tec)
					return;
				self->close();
			});

		// DoH 模式无需后端连接（每个查询独立发起 DoH 请求）.
		if (!m_doh_mode && !m_doh_via_proxy)
		{
			// 直连或 SOCKS5 需要本地 UDP 后端 socket.
			if (!m_connect_udp && !m_websocket_udp &&
				!co_await open_backend())
			{
				close();
				co_return;
			}

			if (m_proxy && !co_await establish_proxy())
			{
				close();
				co_return;
			}

			if (!m_connect_udp && !m_websocket_udp)
			{
				// 直连/SOCKS5：启动后端应答接收循环.
				// capsule 隧道的收发协程由对应 do_connect_* 启动.
				net::co_spawn(m_executor,
					[this, self]() -> net::awaitable<void>
					{
						return recv_loop();
					}, net::detached);
			}
		}

		// 后端就绪，补发等待期间缓存的客户端数据.
		conn_timer.cancel();
		m_ready = true;
		for (auto& p : m_pending)
		{
			bool ok = false;
			if (m_connect_udp || m_websocket_udp)
			{
				push_capsule(p.data(), p.size());
				ok = true;
			}
			else if (m_proxy)
				ok = send_via_socks5(p.data(), p.size());
			else
				ok = send_direct(p.data(), p.size());
			if (ok)
				touch();
		}
		m_pending.clear();

		touch();

		// 启动客户端数据报接收循环.
		net::co_spawn(m_executor,
			[this, self]() -> net::awaitable<void>
			{
				char buf[65535];

				for (; !m_closed;)
				{
					net::ip::udp::endpoint sender;
					boost::system::error_code ec;
					size_t n = co_await m_client_socket.async_receive_from(
						net::buffer(buf), sender, net_awaitable[ec]);
					if (ec)
						break;

					// tunio 会话按客户端三元组复用（1 对 N）：目标变化时
					// 关闭会话，交由引擎为新目标重建会话并重新分流.
					if (sender != m_orig_target)
					{
						close();
						break;
					}

					co_await handle_datagram(buf, n);
					if (m_closed)
						break;
				}

				close();
			}, net::detached);
	}

	// handle_datagram 处理一个客户端数据报（DNS 缓存/DoH/转发）.
	net::awaitable<void> tun_udp_flow::handle_datagram(const char* data, size_t len)
	{
		if (m_closed)
			co_return;

		m_rx_bytes += static_cast<uint64_t>(len);
		if (m_owner)
			m_owner->add_traffic(len, 0);

		// DNS 查询：解析 qname/qtype，禁用 IPv6 时 AAAA 直接回空应答，
		// 其余查询命中缓存后改写事务 ID 直接回包，不再向上游查询.
		std::string cache_key;  // 未命中时保存当前查询键，供 DoH 写入缓存.
		if (m_target.port() == 53 && len > 0)
		{
			std::string query(data, len);
			std::string qname;
			uint16_t qtype = 0;
			bool cd = false;
			bool do_flag = false;
			if (dns_parse_query(query, qname, qtype) && !qname.empty())
			{
				// 禁用 IPv6 解析返回：AAAA 查询直接回 NODATA，不查缓存也不转发上游.
				if (m_option.dns_no_ipv6_ && qtype == DNS_TYPE_AAAA)
				{
					auto resp = dns_build_response(query, 0, {});
					if (!resp.empty())
					{
						AXLOG_DBG << "tun udp dns query: " << qname << " type "
							<< dns_type_to_string(qtype)
							<< ", ipv6 disabled, return empty";
						co_await reply(resp.data(), resp.size(), false);
						touch();
					}
					co_return;
				}

				dns_query_flags(query, cd, do_flag);
				auto cache = m_owner ? m_owner->dns_cache() : nullptr;
				if (cache)
				{
					auto key = dns_cache_key(qname, qtype, cd, do_flag);
					if (auto hit = cache->get(key); hit)
					{
						uint16_t qid = static_cast<uint16_t>(
							(static_cast<uint8_t>(query[0]) << 8) |
							static_cast<uint8_t>(query[1]));
						auto resp = dns_set_id(*hit, qid);
						AXLOG_DBG << "tun udp dns cache hit: " << qname;
						co_await reply(resp.data(), resp.size(), false);
						touch();
						co_return;
					}
					cache_key = key;
				}
			}
		}

		// DoH 模式：经 doh_client 连接池复用 keep-alive 连接发起查询.
		if (m_doh_mode || m_doh_via_proxy)
		{
			auto self = shared_from_this();
			std::string query(data, len);
			net::co_spawn(m_executor,
				[this, self, query = std::move(query),
					key = std::move(cache_key)]()
				-> net::awaitable<void>
				{
					std::string response;
					auto pool = m_owner ? m_owner->doh_pool() : nullptr;
					if (pool)
						response = co_await pool->query(query);
					if (m_closed)
						co_return;
					// 查询失败返回 SERVFAIL, 避免客户端等待超时重试.
					if (response.empty())
						response = dns_build_response(query, 2, {});
					// 缓存写入使用查询侧键（DoH 响应与查询一一对应）.
					if (!key.empty() && dns_cacheable(response))
					{
						auto cache = m_owner ? m_owner->dns_cache() : nullptr;
						if (cache)
						{
							cache->put(key, dns_strip_id(response));
							AXLOG_DBG << "tun udp dns cache put: " << key;
						}
					}
					co_await reply(response.data(), response.size(), false);
					touch();
				}, net::detached);
			co_return;
		}

		// 后端尚未就绪（异步建连/ASSOCIATE 进行中），缓存数据稍后补发.
		if (!m_ready)
		{
			// 超出缓存上限直接丢弃, 防止后端建连缓慢时内存无界增长.
			if (m_pending.size() >= k_max_udp_pending)
				co_return;
			m_pending.emplace_back(data, len);
			co_return;
		}

		bool ok = false;
		if (m_connect_udp || m_websocket_udp)
		{
			// HTTP/WebSocket CONNECT-UDP：封装 DATAGRAM capsule 写入隧道.
			push_capsule(data, len);
			ok = true;
		}
		else if (m_proxy)
			ok = send_via_socks5(data, len);
		else
			ok = send_direct(data, len);

		if (ok)
			touch();
	}

	bool tun_udp_flow::send_direct(const char* data, size_t len)
	{
		boost::system::error_code ec;
		// self_query 时改发国内 DNS（m_send_endp），其余发往原始目标.
		const auto& dst = m_send_endp.port() != 0 ? m_send_endp : m_target;
		m_backend->send_to(net::buffer(data, len), dst, 0, ec);
		if (ec)
		{
			close();
			return false;
		}
		return true;
	}

	bool tun_udp_flow::send_via_socks5(const char* data, size_t len)
	{
		// SOCKS5 UDP 请求头：RSV(2) + FRAG(1) + ATYP(1) + 地址 + 端口.
		auto header = tun_detail::build_socks5_udp_header(m_target);

		if (header.size() + len > 65535)
		{
			close();
			return false;
		}

		std::string buf;
		buf.reserve(header.size() + len);
		buf.append(header);
		buf.append(data, len);

		boost::system::error_code ec;
		m_backend->send_to(net::buffer(buf), m_backend_endp, 0, ec);
		if (ec)
		{
			XLOG_WARN << "tun udp send error: " << ec.message();
			close();
			return false;
		}
		return true;
	}

	void tun_udp_flow::close()
	{
		if (m_closed)
			return;

		m_closed = true;

		m_pending.clear();

		{
			std::lock_guard<std::mutex> lk(m_tx_mutex);
			m_tx_queue.clear();
		}

		if (m_expire)
			m_expire->cancel();
		if (m_tx_signal)
			m_tx_signal->cancel();

		boost::system::error_code ec;
		if (m_backend)
			m_backend->close(ec);
		if (m_control)
			net_tcp_socket(*m_control).close(ec);

		m_client_socket.close();

		if (m_owner)
			m_owner->remove_udp_flow(m_key, this);
	}

	// resolve_proxy_route 判定该 UDP 流是否走上游代理：目标 IP 命中
	// proxy_cidr_ 或查询域名命中 proxy_domains_ 走代理，否则直连.
	// proxy 自身解析 proxy_pass 域名的查询强制直连，避免解析循环.
	bool tun_udp_flow::resolve_proxy_route(bool& self_query)
	{
		bool use_proxy = m_owner && m_owner->ip_match_proxy(m_target.address());

		self_query = false;
		// DNS 查询按查询域名分流：命中 proxy_domains_ 走代理，否则直连.
		if (m_target.port() == 53 && !m_dns_qname.empty())
		{
			// proxy 自身解析 proxy_pass 域名的查询必须直连, 否则会
			// 经代理转发又需要解析该域名, 形成解析循环.
			if (m_option.proxy_pass_)
			{
				auto host = boost::to_lower_copy(
					std::string((*m_option.proxy_pass_).encoded_host()));
				if (!host.empty())
				{
					std::string qname = boost::to_lower_copy(m_dns_qname);
					if (!qname.empty() && qname.back() == '.')
						qname.pop_back();
					self_query = (qname == host);
				}
			}
			if (self_query)
				use_proxy = false;
			else if (!use_proxy)
				use_proxy = m_owner && m_owner->domain_match(m_dns_qname);
		}
		return use_proxy;
	}

	// resolve_dns_route 对 DNS 查询按域名区分国内/国外分流：
	// 国外域名可走 DoH（同服务直连或经代理 CONNECT 隧道）或经代理转发到
	// 国外 DNS，国内域名直连国内 DNS；use_proxy 与 m_target 随之调整.
	void tun_udp_flow::resolve_dns_route(bool& use_proxy, bool self_query)
	{
		m_doh_mode = false;
		m_doh_via_proxy = false;
		if (m_target.port() != 53 || !m_option.proxy_pass_)
			return;

		// proxy 自身解析 proxy_pass 域名的查询（self_query）：改发国内
		// DNS 可靠解析，避免直连 VpnService 注入的 8.8.8.8 等国外 DNS 时
		// 被污染返回 NXDOMAIN. 注意只改发送目标（m_send_endp），响应回包
		// 源地址仍用 m_target（原始 DNS），客户端 socket 与之建立连接.
		if (self_query)
		{
			if (auto dns = tun_detail::pick_dns_server(m_option.dns_domestic_, m_target))
				m_send_endp = *dns;
			return;
		}

		auto scheme = boost::to_lower_copy(
			std::string((*m_option.proxy_pass_).scheme()));

		// 配置了 proxy_domains_（启用域名分流）：按 qname 区分国内/国外 DNS；
		// 未配置时全部按国外域名处理.
		bool foreign = m_option.proxy_domains_.empty() ||
			(m_owner && m_owner->domain_match(m_dns_qname));
		if (foreign)
		{
			// 国外域名：经代理转发 DNS 请求或发起 DoH.
			// DoH 对外层 http(s) 或 ws(s) 代理生效；
			// socks 上游回退为原始查询经 socks 转发.
			if (!m_option.dns_doh_.empty() &&
				(scheme.starts_with("http") ||
					proxy_scheme_is_websocket(scheme)))
			{
				// 与 proxy_pass 同服务的 DoH 直连，否则经代理 CONNECT.
				std::string doh_host;
				if (auto r = urls::parse_uri(m_option.dns_doh_);
					r.has_value())
					doh_host = std::string(r->encoded_host());
				std::string proxy_host = boost::to_lower_copy(
					std::string((*m_option.proxy_pass_).encoded_host()));
				m_doh_via_proxy = !doh_host.empty() &&
					doh_host != proxy_host;
				m_doh_mode = !m_doh_via_proxy;
				use_proxy = false;
			}
			else if (!m_option.dns_foreign_.empty() ||
				!m_option.proxy_domains_.empty())
			{
				// 原始 DNS 数据包经代理转发到国外 DNS（未配置国外 DNS 时
				// 不替换目标，保持 addDnsServer 注入的 8.8.8.8/1.1.1.1）.
				use_proxy = true;
				if (auto dns = tun_detail::pick_dns_server(
					m_option.dns_foreign_, m_target))
					m_target = *dns;
			}
			else
			{
				// 未配置域名分流且国外 DNS/DoH 均留空：把 proxy_pass
				// 尝试作为 DoH 服务器解析客户端查询.
				m_doh_mode = true;
				use_proxy = false;
			}
		}
		else
		{
			// 国内域名：直连国内 DNS 解析.
			use_proxy = false;
			if (auto dns = tun_detail::pick_dns_server(
				m_option.dns_domestic_, m_target))
				m_target = *dns;
		}
	}

	// resolve_udp_transport 判定 UDP 传输模式：http(s) 代理走 RFC 9298
	// CONNECT-UDP，ws/wss 走私有 WebSocket capsule 隧道，socks 代理走
	// UDP ASSOCIATE，其余 scheme 不支持.
	bool tun_udp_flow::resolve_udp_transport()
	{
		auto scheme = boost::to_lower_copy(
			std::string((*m_option.proxy_pass_).scheme()));
		m_connect_udp = scheme.starts_with("http");
		m_websocket_udp = proxy_scheme_is_websocket(scheme);
		if (m_websocket_udp)
		{
			m_proto.store(static_cast<uint8_t>(
				proxy_scheme_use_ssl(scheme) ?
					tun_conn_proto::wss_udp : tun_conn_proto::ws_udp));
		}
		else
		{
			m_proto.store(static_cast<uint8_t>(m_connect_udp ?
				tun_conn_proto::connect_udp : tun_conn_proto::socks5));
		}

		if (!m_connect_udp && !m_websocket_udp &&
			!scheme.starts_with("socks"))
		{
			XLOG_WARN << "tun udp unsupported proxy_pass scheme: " << scheme;
			return false;
		}
		return true;
	}

	// resolve_proxy_mode 综合分流结果确定代理模式：
	// 依次判定走代理与否、DNS 分流，再设置代理标记与传输模式.
	bool tun_udp_flow::resolve_proxy_mode()
	{
		bool self_query = false;
		bool use_proxy = resolve_proxy_route(self_query);

		resolve_dns_route(use_proxy, self_query);

		m_proxy = use_proxy && m_option.proxy_pass_ &&
			!m_doh_mode && !m_doh_via_proxy;
		m_proto.store(static_cast<uint8_t>((m_doh_mode || m_doh_via_proxy) ?
			tun_conn_proto::doh_dns : tun_conn_proto::udp));

		XLOG_DBG << "tun udp flow " << m_client.address().to_string() << ":"
			<< m_client.port() << " -> " << m_target.address().to_string() << ":"
			<< m_target.port()
			<< ((m_doh_mode || m_doh_via_proxy) ? " via doh"
				: (m_proxy ? " via proxy" : " direct"))
			<< (m_dns_qname.empty() ? "" : (", qname=" + m_dns_qname));

		if (!m_proxy)
			return true;

		return resolve_udp_transport();
	}

	net::awaitable<bool> tun_udp_flow::establish_proxy()
	{
		const auto& proxy_url = *m_option.proxy_pass_;

		const auto protect = m_owner ?
			std::function<net::awaitable<bool>(int)>(
				[owner = m_owner](int fd) { return owner->protect_socket(fd); }) :
			std::function<net::awaitable<bool>(int)>();

		auto sock = co_await connect_proxy_pass(
			m_executor, m_option, proxy_url, protect);
		if (!sock.is_open())
			co_return false;

		// HTTP：RFC 9298 CONNECT-UDP；WebSocket：私有 capsule 隧道；
		// SOCKS5：UDP ASSOCIATE.
		if (m_websocket_udp)
			co_return co_await do_connect_websocket_udp(std::move(sock));
		if (m_connect_udp)
			co_return co_await do_connect_udp(std::move(sock));
		co_return co_await do_socks5_associate(std::move(sock));
	}

	net::awaitable<bool> tun_udp_flow::open_backend()
	{
		boost::system::error_code ec;

		// 打开后端 UDP socket（与上游代理或目标通信）.
		m_backend.emplace(m_executor);
		m_backend->open(m_target.protocol(), ec);
		if (ec)
		{
			XLOG_WARN << "tun udp open backend socket: " << ec.message();
			co_return false;
		}

		// 设置 SO_MARK（配合策略路由排除代理自身流量，防止环路）.
		apply_so_mark_if(*m_backend, m_option);

		// Android VpnService 场景: 放行后端 UDP socket, 避免回环进 tun.
		// 放行失败（launcher 未就绪等）时不得继续使用, 否则流量回环.
		if (m_owner &&
			!co_await m_owner->protect_socket(m_backend->native_handle()))
		{
			XLOG_WARN << "tun udp backend socket not protected";
			co_return false;
		}

		m_backend->bind(net::ip::udp::endpoint(m_target.protocol(), 0), ec);
		if (ec)
		{
			XLOG_WARN << "tun udp bind backend socket: " << ec.message();
			co_return false;
		}

		co_return true;
	}

	net::awaitable<bool> tun_udp_flow::do_socks5_associate(tcp::socket sock)
	{
		auto self = shared_from_this();
		boost::system::error_code ec;

		const auto& proxy_url = *m_option.proxy_pass_;

		// 与 do_connect_udp 一致：SSL 代理（scheme 以 s 结尾，如 socks5s/
		// https）先建立 TLS 再执行 SOCKS5 握手，否则明文发往 TLS 端口
		// 会被对端直接 RST.
		if (proxy_use_ssl(proxy_url, m_option))
		{
			if (!m_owner)
				co_return false;
			std::string proxy_host(proxy_url.host_address());
			auto sni = m_option.proxy_ssl_name_.empty() ?
				proxy_host : m_option.proxy_ssl_name_;
			auto res = co_await m_owner->make_ssl_socket(sock, sni, m_control);
			if (res.has_error())
			{
				XLOG_WARN << "tun udp make_ssl_socket: "
					<< res.error().message();
				co_return false;
			}
			XLOG_DBG << "tun udp SSL handshake with " << sni << " succeeded";
		}
		else
		{
			m_control.emplace(init_proxy_stream(std::move(sock)));
		}

		socks_client_option opt;
		// UDP ASSOCIATE 的 DST.ADDR/DST.PORT 表示客户端发送数据报
		// 所用的端点；此时尚未创建 UDP socket，使用全零占位.
		opt.target_host = "0.0.0.0";
		opt.target_port = 0;
		opt.proxy_hostname = false;
		opt.command = SOCKS5_CMD_UDP;
		opt.username = std::string(proxy_url.user());
		opt.password = std::string(proxy_url.password());

		auto backend = co_await async_socks_handshake(
			*m_control, opt, net_awaitable[ec]);
		if (ec || !backend)
		{
			XLOG_WARN << "tun udp SOCKS5 associate: " << ec.message();
			co_return false;
		}

		m_backend_endp = *backend;

		// 某些代理只返回端口，地址为空，此时用控制连接的远端地址补全.
		if (m_backend_endp.address().is_unspecified())
		{
			m_backend_endp.address(tcp_remote_endpoint(*m_control).address());
		}
		if (m_backend_endp.address().is_unspecified() ||
			m_backend_endp.port() == 0)
		{
			XLOG_WARN << "tun udp invalid SOCKS5 relay endpoint: "
				<< m_backend_endp;
			co_return false;
		}

		// 保持控制连接存活，断开时关闭整个会话.
		net::co_spawn(m_executor,
			[this, self]() -> net::awaitable<void>
			{
				return control_loop();
			}, net::detached);

		co_return true;
	}

	net::awaitable<bool> tun_udp_flow::do_connect_udp(tcp::socket sock)
	{
		auto self = shared_from_this();

		const auto& proxy_url = *m_option.proxy_pass_;
		std::string proxy_host(proxy_url.host_address());

		// 判断是否使用 SSL：显式配置 proxy_pass_ssl 或 scheme 以 's' 结尾（https/wss）.
		if (proxy_use_ssl(proxy_url, m_option))
		{
			auto sni = m_option.proxy_ssl_name_.empty() ?
				proxy_host : m_option.proxy_ssl_name_;
			auto res = co_await m_owner->make_ssl_socket(sock, sni, m_control);
			if (res.has_error())
			{
				XLOG_WARN << "tun udp make_ssl_socket: " << res.error().message();
				co_return false;
			}

			XLOG_DBG << "tun udp SSL handshake with " << sni << " succeeded";
		}
		else
		{
			m_control.emplace(init_proxy_stream(std::move(sock)));
		}

		// 发送 RFC 9298 CONNECT-UDP 请求并校验 101 响应.
		if (!co_await send_connect_udp_request())
			co_return false;
		if (!co_await read_connect_udp_response())
			co_return false;

		// 启动串行发送协程与 capsule 接收循环.
		start_data_loops();

		co_return true;
	}

	net::awaitable<bool> tun_udp_flow::do_connect_websocket_udp(tcp::socket sock)
	{
		boost::system::error_code ec;
		const auto& proxy_url = *m_option.proxy_pass_;

		if (proxy_use_ssl(proxy_url, m_option))
		{
			if (!m_owner)
				co_return false;

			auto proxy_host = std::string(proxy_url.host_address());
			auto sni = m_option.proxy_ssl_name_.empty() ?
				proxy_host : m_option.proxy_ssl_name_;
			auto res = co_await m_owner->make_ssl_socket(sock, sni, m_control);
			if (res.has_error())
			{
				XLOG_WARN << "tun websocket udp make_ssl_socket: "
					<< res.error().message();
				co_return false;
			}
		}
		else
		{
			m_control.emplace(init_proxy_stream(std::move(sock)));
		}

		auto options = make_websocket_handshake_options(proxy_url,
			m_target.address().to_string(), m_target.port(), true);
		ec = co_await async_websocket_proxy_handshake(
			*m_control, std::move(options));
		if (ec)
		{
			XLOG_WARN << "tun websocket udp handshake: " << ec.message();
			close_stream(*m_control);
			co_return false;
		}

		XLOG_DBG << "tun websocket udp established to target: " << m_target;
		start_data_loops();
		co_return true;
	}

	// send_connect_udp_request 发送 RFC 9298 CONNECT-UDP 请求（absolute-form URI）.
	net::awaitable<bool> tun_udp_flow::send_connect_udp_request()
	{
		boost::system::error_code ec;

		const auto& proxy_url = *m_option.proxy_pass_;
		auto req = tun_detail::build_connect_udp_request(
			proxy_url, m_target, proxy_use_ssl(proxy_url, m_option));

		auto& http_sock = *m_control;

		co_await http::async_write(http_sock, req, net_awaitable[ec]);
		if (ec)
		{
			XLOG_WARN << "tun udp send connect-udp request: " << ec.message();
			co_return false;
		}

		co_return true;
	}

	// read_connect_udp_response 读取并校验 CONNECT-UDP 响应（须为 101）.
	net::awaitable<bool> tun_udp_flow::read_connect_udp_response()
	{
		boost::system::error_code ec;

		auto& http_sock = *m_control;

		http::response_parser<http::empty_body> resp_parser;
		resp_parser.skip(true);

		co_await http::async_read_header(
			http_sock, m_capsule_buffer, resp_parser, net_awaitable[ec]);
		if (ec)
		{
			XLOG_WARN << "tun udp read connect-udp response: " << ec.message();
			co_return false;
		}

		auto resp = resp_parser.release();
		if (!is_connect_udp_upgrade_response(resp))
		{
			XLOG_WARN << "tun udp invalid connect-udp response: "
				<< static_cast<int>(resp.result());
			co_return false;
		}

		XLOG_DBG << "tun udp connect-udp established to target: " << m_target;
		co_return true;
	}

	// start_data_loops 启动串行发送协程与 capsule 接收循环.
	void tun_udp_flow::start_data_loops()
	{
		if (m_data_loops_started)
			return;
		m_data_loops_started = true;

		auto self = shared_from_this();

		m_tx_signal.emplace(m_executor);

		net::co_spawn(m_executor,
			[this, self]() -> net::awaitable<void>
			{
				return tx_loop();
			}, net::detached);

		net::co_spawn(m_executor,
			[this, self]() -> net::awaitable<void>
			{
				return recv_connect_udp_loop();
			}, net::detached);
	}

	void tun_udp_flow::push_capsule(const char* data, size_t len)
	{
		std::array<uint8_t, datagram_capsule_max_size> buf{};

		AXLOG_DBG << "tun udp tx capsule to " << m_target.address().to_string()
			<< ":" << m_target.port() << " len=" << len;

		auto pos = encode_datagram_capsule(data, len, buf.data(), buf.size());
		if (pos == 0)
		{
			close();
			return;
		}

		{
			std::lock_guard<std::mutex> lk(m_tx_mutex);
			// 发送协程消费速度跟不上时丢弃新包, 防止队列无界增长.
			if (m_tx_queue.size() >= k_max_udp_pending)
				return;
			m_tx_queue.emplace_back(buf.data(), buf.data() + pos);
		}

		if (m_tx_signal)
			m_tx_signal->cancel();
	}

	net::awaitable<void> tun_udp_flow::tx_loop()
	{
		for (; !m_closed;)
		{
			std::vector<char> item;

			{
				std::lock_guard<std::mutex> lk(m_tx_mutex);
				if (!m_tx_queue.empty())
				{
					item = std::move(m_tx_queue.front());
					m_tx_queue.pop_front();
				}
			}

			if (item.empty())
			{
				boost::system::error_code ec;
				m_tx_signal->expires_at(net::steady_timer::time_point::max());
				co_await m_tx_signal->async_wait(net_awaitable[ec]);
				continue;
			}

			boost::system::error_code ec;
			if (m_websocket_udp)
			{
				ec = co_await async_websocket_write_binary(
					*m_control, net::buffer(item));
			}
			else
			{
				co_await net::async_write(
					*m_control, net::buffer(item), net_awaitable[ec]);
			}
			if (ec)
				break;
		}

		close();
	}

	net::awaitable<void> tun_udp_flow::recv_connect_udp_loop()
	{
		if (!m_control)
			co_return;

		beast::flat_buffer ws_message;
		for (; !m_closed;)
		{
			boost::system::error_code ec;
			if (m_websocket_udp)
			{
				ec = co_await async_websocket_read_binary(
					*m_control, ws_message);
				if (ec)
					break;

				auto data = ws_message.data();
				std::string_view payload;
				if (!decode_datagram_message(std::string_view(
					static_cast<const char*>(data.data()), data.size()),
					payload))
					continue;

				touch();
				co_await reply(payload.data(), payload.size());
			}
			else
			{
				auto [capsule_type, capsule_value] = co_await
					read_capsule_from_stream(
						*m_control, m_capsule_buffer, ec);
				if (ec)
					break;

				AXLOG_DBG << "tun udp rx capsule type=" << capsule_type
					<< " len=" << capsule_value.size();

				// 仅处理 DATAGRAM capsule（RFC 9297）.
				if (capsule_type != udp_proxy_capsule_type)
					continue;

				// 解析 DATAGRAM capsule, 取出 UDP payload.
				std::string_view payload;
				if (!decode_datagram_capsule(capsule_value, payload))
					continue;

				touch();
				co_await reply(payload.data(), payload.size());
			}
		}

		close();
	}


	net::awaitable<void> tun_udp_flow::control_loop()
	{
		if (!m_control)
			co_return;

		char buf[64];

		for (; !m_closed;)
		{
			boost::system::error_code ec;
			co_await m_control->async_read_some(
				net::buffer(buf), net_awaitable[ec]);
			if (ec)
				break;
		}

		// 控制连接断开后 ASSOCIATE 会话失效，关闭整个 flow.
		close();
	}

	net::awaitable<void> tun_udp_flow::recv_loop()
	{
		if (!m_backend)
			co_return;

		char buf[65535];

		for (; !m_closed;)
		{
			boost::system::error_code ec;
			size_t n = co_await m_backend->async_receive(
				net::buffer(buf), net_awaitable[ec]);
			if (ec)
				break;

			touch();

			if (!m_proxy)
			{
				// 直连：应答即为目标返回的数据.
				co_await reply(buf, n);
				continue;
			}

			// SOCKS5 UDP 应答头：RSV(2) + FRAG(1) + ATYP(1) + 地址 + 端口.
			size_t header_size = tun_detail::socks5_udp_header_size(buf, n);
			if (header_size == 0 || n <= header_size)
				continue;

			co_await reply(buf + header_size, n - header_size);
		}

		close();
	}

	// reply 向客户端回包（源地址为会话目标 m_target）.
	net::awaitable<void> tun_udp_flow::reply(
		const char* data, size_t len, bool cache_resp)
	{
		if (!m_owner)
			co_return;

		m_tx_bytes += static_cast<uint64_t>(len);
		if (m_owner)
			m_owner->add_traffic(0, len);

		// DNS 响应回包时记录 A/AAAA 解析结果供数据面按域名分流，
		// 并写入查询结果缓存.
		if (m_target.port() == 53)
		{
			m_owner->record_dns_answer(data, len);
			if (cache_resp)
				cache_dns_response(data, len);
		}

		// 目标 -> 客户端方向：经 tunio 会话回写，源地址为 m_target.
		std::string payload(data, len);
		boost::system::error_code ec;
		co_await m_client_socket.async_send_to(
			m_target, net::buffer(payload), net_awaitable[ec]);
		if (ec)
		{
			XLOG_WARN << "tun udp reply error: " << ec.message();
			close();
		}
	}

	// cache_dns_response 将 DNS 响应写入查询结果缓存，键从响应报文
	// 回显的 question 区解析（域名 + 类型 + CD/DO 标志）.
	void tun_udp_flow::cache_dns_response(const char* data, size_t len) noexcept
	{
		auto cache = m_owner ? m_owner->dns_cache() : nullptr;
		if (!cache || len < 2)
			return;

		std::string resp(data, len);
		std::string qname;
		uint16_t qtype = 0;
		if (!dns_parse_query(resp, qname, qtype) || qname.empty())
			return;
		if (!dns_cacheable(resp))
			return;

		bool cd = false;
		bool do_flag = false;
		dns_query_flags(resp, cd, do_flag);
		auto key = dns_cache_key(qname, qtype, cd, do_flag);
		cache->put(key, dns_strip_id(resp));
		AXLOG_DBG << "tun udp dns cache put: " << key;
	}

	void tun_udp_flow::touch()
	{
		if (m_closed || !m_expire)
			return;

		// 每次活动重置超时计时，超时后关闭会话.
		m_expire->expires_after(std::chrono::seconds(tun_detail::udp_idle_timeout(m_option)));
		m_expire->async_wait(
			[this](const boost::system::error_code& ec)
			{
				if (ec)
					return;  // 计时器被取消（新活动重置）.
				close();
			});
	}

#endif // defined(__linux__) || defined(__APPLE__) || defined(_WIN32)

} // namespace proxy
