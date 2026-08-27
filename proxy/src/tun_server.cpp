//
// tun_server.cpp
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/tun_server.hpp"

#include "proxy/dns_server.hpp"
#include "proxy/http_proxy_client.hpp"
#include "proxy/logging.hpp"
#include "proxy/proxy_util.hpp"
#include "proxy/socks_client.hpp"
#include "proxy/socks_io.hpp"
#include "proxy/wintun_install.hpp"

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/functional/hash.hpp>

#if defined(__linux__)
# include <arpa/inet.h>
# include <net/if.h>
# include <sys/ioctl.h>
# include <sys/socket.h>
# include <unistd.h>
#endif

#include <algorithm>
#include <chrono>
#include <cstring>
#include <deque>
#include <future>
#include <string>
#include <vector>

namespace proxy {

	namespace {

		// udp 会话空闲超时（配置不大于 0 时用默认 300 秒兜底）.
		int udp_idle_timeout(const proxy_server_option& opt) noexcept
		{
			return opt.udp_timeout_ > 0 ? opt.udp_timeout_ : 300;
		}

#if defined(__linux__)
		// 查询 tun 设备当前 IPv4 地址（外部 ip addr add 配置），
		// 供环路防护识别本地地址.
		void query_tun_device_ip(const std::string& dev, std::string& ip)
		{
			const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
			if (fd < 0)
				return;
			struct ifreq ifr {};
			std::strncpy(ifr.ifr_name, dev.c_str(), IFNAMSIZ - 1);
			if (::ioctl(fd, SIOCGIFADDR, &ifr) == 0)
			{
				const auto* sin = reinterpret_cast<const sockaddr_in*>(
					&ifr.ifr_addr);
				char buf[INET_ADDRSTRLEN] = {};
				if (::inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf)))
					ip = buf;
			}
			::close(fd);
		}

		// 判断客户端地址是否为 tun 设备自身地址：tun 接管路由后，未绕过
		// tun 的出站连接会以本地地址为源回环进 tun（环路风暴的根源）.
		// 每次调用查询设备地址（ioctl 开销极小），1 秒内缓存避免高频查询.
		bool is_tun_loopback_source(const net::ip::address& addr,
			const std::string& dev) noexcept
		{
			static std::chrono::steady_clock::time_point last;
			static std::string cached;
			const auto now = std::chrono::steady_clock::now();
			if (now - last > std::chrono::seconds(1))
			{
				cached.clear();
				query_tun_device_ip(dev, cached);
				last = now;
			}
			if (cached.empty())
				return false;
			boost::system::error_code ec;
			const auto local = net::ip::make_address(cached, ec);
			return !ec && local == addr;
		}
#endif

		// 构造 SOCKS5 UDP 请求/应答头（RSV + FRAG + ATYP + 地址 + 端口）.
		std::string build_socks5_udp_header(
			const net::ip::udp::endpoint& target)
		{
			std::string header;

			if (target.address().is_v4())
			{
				header.resize(10);
				char* hp = header.data();
				io_util::write<uint16_t>(0, hp);  // RSV.
				*hp++ = 0;                        // FRAG.
				*hp++ = SOCKS5_ATYP_IPV4;
				auto addr = target.address().to_v4().to_bytes();
				std::memcpy(hp, addr.data(), 4);
				hp += 4;
				io_util::write<uint16_t>(target.port(), hp);
			}
			else
			{
				header.resize(22);
				char* hp = header.data();
				io_util::write<uint16_t>(0, hp);  // RSV.
				*hp++ = 0;                        // FRAG.
				*hp++ = SOCKS5_ATYP_IPV6;
				auto addr = target.address().to_v6().to_bytes();
				std::memcpy(hp, addr.data(), 16);
				hp += 16;
				io_util::write<uint16_t>(target.port(), hp);
			}

			return header;
		}

		// 解析 SOCKS5 UDP 应答头，返回头部长度；非法包头返回 0.
		size_t socks5_udp_header_size(const char* data, size_t len) noexcept
		{
			if (len < 4 || static_cast<uint8_t>(data[2]) != 0)
				return 0;  // 不支持分片（FRAG 非 0）.

			size_t header_size = 0;
			switch (static_cast<uint8_t>(data[3]))
			{
			case SOCKS5_ATYP_IPV4:
				header_size = 10;
				break;
			case SOCKS5_ATYP_DOMAINNAME:
				if (len < 5)
					return 0;
				header_size = 7 + static_cast<uint8_t>(data[4]);
				break;
			case SOCKS5_ATYP_IPV6:
				header_size = 22;
				break;
			default:
				return 0;
			}

			return header_size <= len ? header_size : 0;
		}

		// 解析并连接目标（直连），成功返回已连接的 socket；失败返回未打开的 socket.
		net::awaitable<tcp::socket> connect_direct(
			net::any_io_executor executor,
			const proxy_server_option& opt,
			const net::ip::address& dst, uint16_t port,
			const std::function<net::awaitable<bool>(int)>& protect)
		{
			boost::system::error_code ec;

			tcp::socket sock(executor);
			tcp::endpoint endp(dst, port);

			sock.open(endp.protocol(), ec);
			if (!ec)
			{
				apply_so_mark_if(sock, opt);

				// Android VpnService 场景必须先放行再 connect：否则 SYN 会按
				// 全隧道路由回环进 tun，被当成新连接再次转发，形成环路.
				if (protect && !co_await protect(sock.native_handle()))
				{
					XLOG_WARN << "tun connect direct not protected: "
						<< dst.to_string() << ":" << port;
					co_return tcp::socket(executor);
				}

				co_await sock.async_connect(endp, net_awaitable[ec]);
			}
			if (ec)
			{
				XLOG_WARN << "tun connect direct: " << dst.to_string()
					<< ":" << port << ", error: " << ec.message();
				co_return tcp::socket(executor);
			}

			co_return sock;
		}

		// 构造 RFC 9298 CONNECT-UDP 请求（absolute-form URI）.
		http::request<http::empty_body> build_connect_udp_request(
			const urls::url& proxy_url,
			const net::ip::udp::endpoint& target)
		{
			std::string proxy_host(proxy_url.encoded_host());
			uint16_t proxy_port = proxy_url.port_number();
			if (proxy_port == 0)
				proxy_port = proxy_pass_default_port(proxy_url);

			std::string target_host = target.address().to_string();
			if (target.address().is_v6())
				boost::replace_all(target_host, ":", "%3A");

			std::string uri =
				"https://" + proxy_host + ":" + std::to_string(proxy_port) +
				"/.well-known/masque/udp/" + target_host + "/" +
				std::to_string(target.port()) + "/";

			http::request<http::empty_body> req{ http::verb::get, uri, 11 };
			req.set(http::field::host, proxy_host + ":" + std::to_string(proxy_port));
			req.set(http::field::connection, "Upgrade");
			req.set(http::field::upgrade, "connect-udp");
			req.set("Capsule-Protocol", "?1");

			// 需要认证时添加 Proxy-Authorization 头.
			if (!proxy_url.user().empty())
			{
				const auto userinfo =
					std::string(proxy_url.user()) + ":" +
					std::string(proxy_url.password());
				req.set(http::field::proxy_authorization,
					"Basic " + strutil::base64_encode(userinfo));
			}

			return req;
		}

		// 从 DNS 服务器列表中选择与 target 同地址族的条目（端口 53），
		// 无同族条目时取首个可解析地址，全部失败返回空.
		std::optional<net::ip::udp::endpoint> pick_dns_server(
			const std::vector<std::string>& dns_list,
			const net::ip::udp::endpoint& target) noexcept
		{
			for (const auto& s : dns_list)
			{
				boost::system::error_code ec;
				auto addr = net::ip::make_address(s, ec);
				if (ec)
					continue;
				if (addr.is_v4() == target.address().is_v4())
					return net::ip::udp::endpoint(addr, 53);
			}
			for (const auto& s : dns_list)
			{
				boost::system::error_code ec;
				auto addr = net::ip::make_address(s, ec);
				if (!ec)
					return net::ip::udp::endpoint(addr, 53);
			}
			return std::nullopt;
		}

		// parse_dns_question 解析问题区，取第一个问题域名填充 qname，
		// 成功返回解析后的偏移位置，失败返回 nullptr.
		const char* parse_dns_question(const char* p, const char* end,
			const char* msg_start, uint16_t qdcount, std::string& qname)
		{
			for (uint16_t i = 0; i < qdcount; ++i)
			{
				auto [name, np] = dns_parse_name(p, end, msg_start);
				if (!np || np + 4 > end)
					return nullptr;

				if (i == 0)
					qname = name;
				p = np + 4;
			}
			if (qdcount == 0 || qname.empty())
				return nullptr;

			// 去掉末尾的 '.'.
			if (!qname.empty() && qname.back() == '.')
				qname.pop_back();
			return p;
		}

		// parse_dns_answer 解析单条 Answer 记录，A/AAAA 记录加入 answers，
		// 成功返回下一条记录的偏移位置，失败返回 nullptr.
		const char* parse_dns_answer(const char* p, const char* end,
			const char* msg_start,
			std::vector<std::pair<net::ip::address, uint32_t>>& answers)
		{
			if (p + 10 > end)
				return nullptr;

			auto [name, np] = dns_parse_name(p, end, msg_start);
			if (!np || np + 10 > end)
				return nullptr;

			const char* type_p = np;
			const char* ttl_p = np + 4;
			const char* rdl_p = np + 8;
			uint16_t type = io_util::read<uint16_t>(type_p);
			uint32_t ttl = io_util::read<uint32_t>(ttl_p);
			uint16_t rdlength = io_util::read<uint16_t>(rdl_p);
			const char* rdata = np + 10;
			if (rdata + rdlength > end)
				return nullptr;

			if (type == 1 && rdlength == 4)
			{
				net::ip::address_v4::bytes_type bytes;
				std::memcpy(bytes.data(), rdata, 4);
				answers.emplace_back(net::ip::address_v4(bytes), ttl);
			}
			else if (type == 28 && rdlength == 16)
			{
				net::ip::address_v6::bytes_type bytes;
				std::memcpy(bytes.data(), rdata, 16);
				answers.emplace_back(net::ip::address_v6(bytes), ttl);
			}

			return rdata + rdlength;
		}

		// parse_dns_response 解析 DNS 响应报文中的问题域名与 A/AAAA 答案.
		// 成功返回 true，qname/answers 为解析结果（answers 元素为 IP 与 TTL）.
		bool parse_dns_response(const char* data, size_t len,
			std::string& qname,
			std::vector<std::pair<net::ip::address, uint32_t>>& answers)
		{
			if (len < 12)
				return false;

			const char* msg_start = data;
			const char* end = data + len;

			const char* qd_p = data + 4;
			const char* an_p = data + 6;
			uint16_t qdcount = io_util::read<uint16_t>(qd_p);
			uint16_t ancount = io_util::read<uint16_t>(an_p);

			// 解析问题区，取第一个问题域名.
			const char* p = parse_dns_question(
				data + 12, end, msg_start, qdcount, qname);
			if (!p)
				return false;

			// 遍历 Answer 区提取 A/AAAA 记录.
			for (uint16_t i = 0; i < ancount; ++i)
			{
				p = parse_dns_answer(p, end, msg_start, answers);
				if (!p)
					break;
			}

			return !answers.empty();
		}

	} // namespace

	//////////////////////////////////////////////////////////////////////////
	// tun_tcp_flow

#if defined(__linux__) || defined(__APPLE__) || defined(_WIN32)

	tun_tcp_flow::tun_tcp_flow(net::any_io_executor executor,
		const std::shared_ptr<tun_server>& owner,
		const proxy_server_option& opt,
		tcp_flow_key key,
		tunio::tun_stream stream)
		: m_executor(std::move(executor))
		, m_owner(owner)
		, m_option(opt)
		, m_key(std::move(key))
		, m_conn_id(owner ? owner->next_conn_id() : 0)
		, m_client(stream.remote_endpoint())
		, m_target(stream.original_destination())
		, m_started(std::chrono::steady_clock::now())
		, m_proto(static_cast<uint8_t>(tun_conn_proto::tcp))
		, m_client_stream(std::move(stream))
		, m_upstream(init_proxy_stream(m_executor))
	{}

	tun_tcp_flow::~tun_tcp_flow()
	{
		close();
	}

	void tun_tcp_flow::start()
	{
		auto self = shared_from_this();

		net::co_spawn(m_executor,
			[this, self]() -> net::awaitable<void>
			{
				return do_connect();
			}, net::detached);
	}

	net::awaitable<void> tun_tcp_flow::do_connect()
	{
		auto self = shared_from_this();

		const std::string target_host = m_target.address().to_string();
		const uint16_t target_port = m_target.port();

		// 分流判定：命中 proxy_cidr_ 或代理域名解析缓存走上游代理，否则直连.
		const bool use_proxy = m_owner && m_owner->ip_match_proxy(m_target.address());

		XLOG_DBG << "tun tcp connect " << target_host << ":" << target_port
			<< (use_proxy ? " via proxy" : " direct")
			<< ", cidr_size=" << m_option.proxy_cidr_.size();

		if (!co_await establish_upstream(target_host, target_port, use_proxy))
		{
			// 后端连接失败：向客户端发送 RST 并关闭.
			m_client_stream.reset();
			close();
			co_return;
		}

		m_connected = true;

		XLOG_DBG << "tun tcp established " << target_host << ":" << target_port;

		// 启动双向数据搬运协程.
		net::co_spawn(m_executor,
			[this, self]() -> net::awaitable<void>
			{
				return tx_loop();
			}, net::detached);

		net::co_spawn(m_executor,
			[this, self]() -> net::awaitable<void>
			{
				return rx_loop();
			}, net::detached);
	}

	// establish_upstream 依据分流结果经上游代理或直连建立连接，
	// 成功后设置 m_upstream 并返回 true.
	net::awaitable<bool> tun_tcp_flow::establish_upstream(
		const std::string& target_host, uint16_t target_port, bool use_proxy)
	{
		const auto protect = m_owner ?
			std::function<net::awaitable<bool>(int)>(
				[owner = m_owner](int fd) { return owner->protect_socket(fd); }) :
			std::function<net::awaitable<bool>(int)>();

		if (use_proxy && m_option.proxy_pass_)
			co_return co_await establish_via_proxy(
				target_host, target_port, protect);

		co_return co_await establish_direct(target_port, protect);
	}

	// establish_via_proxy 经上游代理建立连接：按 scheme 记录协议类型，
	// 连接 proxy_pass 后视 SSL 代理先建 TLS，再完成代理协议握手.
	net::awaitable<bool> tun_tcp_flow::establish_via_proxy(
		const std::string& target_host, uint16_t target_port,
		const std::function<net::awaitable<bool>(int)>& protect)
	{
		const auto& proxy_url = *m_option.proxy_pass_;
		auto scheme = boost::to_lower_copy(
			std::string(proxy_url.scheme()));
		if (scheme.starts_with("socks"))
			m_proto.store(static_cast<uint8_t>(tun_conn_proto::socks5));
		else if (proxy_use_ssl(proxy_url, m_option))
			m_proto.store(static_cast<uint8_t>(tun_conn_proto::https));
		else
			m_proto.store(static_cast<uint8_t>(tun_conn_proto::http));

		// 优先从 proxy_pass 预选连接池获取已建立的连接（TCP/TLS 已完成）.
		std::shared_ptr<proxy_pass_pool> pool;
		if (m_option.proxy_pass_pool_size_ > 0 && m_owner)
			pool = m_owner->proxy_conn_pool();
		if (pool)
		{
			auto conn = co_await pool->acquire();
			if (conn && conn->is_open())
			{
				m_upstream = std::move(*conn);
				if (co_await do_proxy_handshake(proxy_url))
				{
					XLOG_DBG << "tun tcp proxy from pool: "
						<< target_host << ":" << target_port;
					co_return true;
				}

				// 池中连接握手失败（代理端可能已断开），丢弃并回退新建连接.
				XLOG_WARN << "tun tcp proxy pool handshake failed, "
					"fallback connect: " << target_host << ":"
					<< target_port;
				m_upstream = init_proxy_stream(m_executor);
			}
			else
			{
				XLOG_WARN << "tun tcp proxy pool acquire failed, "
					"fallback connect: " << target_host << ":"
					<< target_port;
			}
		}

		auto sock = co_await connect_proxy_pass(
			m_executor, m_option, proxy_url, protect);
		if (!sock.is_open())
		{
			XLOG_WARN << "tun tcp connect proxy_pass failed: "
				<< target_host << ":" << target_port;
			co_return false;
		}

		XLOG_DBG << "tun tcp proxy connected: "
			<< target_host << ":" << target_port;

		// https/wss 等 SSL 代理需要先建立 TLS 再执行代理握手.
		if (proxy_use_ssl(proxy_url, m_option))
		{
			if (!m_owner)
				co_return false;

			std::string proxy_host(proxy_url.encoded_host());
			auto sni = m_option.proxy_ssl_name_.empty() ?
				proxy_host : m_option.proxy_ssl_name_;
			std::optional<variant_stream_type> ssl_stream;
			auto res = co_await m_owner->make_ssl_socket(
				sock, sni, ssl_stream);
			if (res.has_error())
			{
				XLOG_WARN << "tun tcp make_ssl_socket: "
					<< res.error().message();
				co_return false;
			}
			m_upstream = std::move(*ssl_stream);
		}
		else
		{
			m_upstream = init_proxy_stream(std::move(sock));
		}

		co_return co_await do_proxy_handshake(proxy_url);
	}

	// establish_direct 直连目标建立连接，成功设置 m_upstream 并返回 true.
	net::awaitable<bool> tun_tcp_flow::establish_direct(
		uint16_t target_port,
		const std::function<net::awaitable<bool>(int)>& protect)
	{
		auto sock = co_await connect_direct(
			m_executor, m_option, m_target.address(), target_port, protect);
		if (!sock.is_open())
			co_return false;

		m_upstream = init_proxy_stream(std::move(sock));
		co_return true;
	}

	net::awaitable<bool> tun_tcp_flow::do_proxy_handshake(const urls::url& proxy_url)
	{
		boost::system::error_code ec;

		const std::string target_host = m_target.address().to_string();
		const uint16_t target_port = m_target.port();
		auto scheme = boost::to_lower_copy(std::string(proxy_url.scheme()));

		if (scheme.starts_with("socks"))
		{
			socks_client_option opt;
			opt.target_host = target_host;
			opt.target_port = target_port;
			// IP 目标直接以地址类型发送，域名目标交由代理解析.
			opt.proxy_hostname = !m_target.address().is_v4() &&
				!m_target.address().is_v6();
			opt.username = std::string(proxy_url.user());
			opt.password = std::string(proxy_url.password());

			co_await async_socks_handshake(m_upstream, opt, net_awaitable[ec]);
		}
		else if (scheme.starts_with("http"))
		{
			http_proxy_client_option opt;
			opt.target_host = target_host;
			opt.target_port = target_port;
			opt.username = std::string(proxy_url.user());
			opt.password = std::string(proxy_url.password());

			co_await async_http_proxy_handshake(m_upstream, opt, net_awaitable[ec]);
		}
		else
		{
			XLOG_WARN << "tun unsupported proxy_pass scheme: " << scheme;
			co_return false;
		}

		if (ec)
		{
			XLOG_WARN << "tun proxy_pass handshake: "
				<< std::string(proxy_url.encoded_host())
				<< ", error: " << ec.message();
			co_return false;
		}

		co_return true;
	}

	// tx_loop 读取客户端（tun_stream）数据并转发到上游.
	net::awaitable<void> tun_tcp_flow::tx_loop()
	{
		char buffer[16384];

		for (; !m_closed;)
		{
			boost::system::error_code ec;
			size_t n = co_await m_client_stream.async_read_some(
				net::buffer(buffer), net_awaitable[ec]);
			if (ec)
			{
				// 客户端流异常（RST/关闭）：整体关闭连接.
				close();
				co_return;
			}

			if (n == 0)
			{
				// 客户端 FIN：半关闭上游写方向，等待上游 EOF 后整体关闭.
				m_client_fin = true;
				if (m_upstream_eof)
				{
					close();
					co_return;
				}

				boost::system::error_code sec;
				net_tcp_socket(m_upstream).shutdown(
					tcp::socket::shutdown_send, sec);
				co_return;
			}

			m_rx_bytes += static_cast<uint64_t>(n);
			if (m_owner)
				m_owner->add_traffic(n, 0);

			XLOG_DBG << "tun tcp tx " << m_target.address().to_string() << ":"
				<< m_target.port() << " len=" << n;

			co_await net::async_write(
				m_upstream, net::buffer(buffer, n), net_awaitable[ec]);
			if (ec)
			{
				if (ec == net::error::eof)
					handle_upstream_eof(ec, 0);
				else
					on_upstream_closed();
				co_return;
			}
		}

		co_return;
	}

	// rx_loop 读取上游数据并转发到客户端（tun_stream）.
	net::awaitable<void> tun_tcp_flow::rx_loop()
	{
		char buffer[8192];

		for (; !m_closed;)
		{
			boost::system::error_code ec;
			size_t n = co_await m_upstream.async_read_some(
				net::buffer(buffer), net_awaitable[ec]);

			if (ec || n == 0)
			{
				handle_upstream_eof(ec, n);
				co_return;
			}

			m_tx_bytes += static_cast<uint64_t>(n);
			if (m_owner)
				m_owner->add_traffic(0, n);

			XLOG_DBG << "tun tcp rx " << m_target.address().to_string() << ":"
				<< m_target.port() << " len=" << n;

			co_await write_to_client(buffer, n);
		}

		co_return;
	}

	// write_to_client 将数据写入客户端流；引擎按 MSS 自动分片，
	// 客户端窗口满时等待 ACK 后继续.
	net::awaitable<void> tun_tcp_flow::write_to_client(const char* data, size_t len)
	{
		for (size_t off = 0; off < len && !m_closed;)
		{
			boost::system::error_code ec;
			size_t n = co_await m_client_stream.async_write_some(
				net::buffer(data + off, len - off), net_awaitable[ec]);
			if (ec)
			{
				// 客户端流不可写（已关闭/RST）：整体关闭连接.
				close();
				co_return;
			}
			off += n;
		}
	}

	// handle_upstream_eof 处理上游读取结束：EOF 向客户端发 FIN，
	// 异常向客户端发 RST 并关闭.
	void tun_tcp_flow::handle_upstream_eof(boost::system::error_code ec, size_t n)
	{
		if (ec == net::error::eof || n == 0)
		{
			// 上游关闭：向客户端发送 FIN（半关闭发送侧）.
			if (!m_upstream_eof)
			{
				m_upstream_eof = true;

				boost::system::error_code sec;
				m_client_stream.shutdown(
					net::ip::tcp::socket::shutdown_send, sec);
			}
			if (m_client_fin)
				close();
		}
		else
		{
			// 上游异常：向客户端发送 RST 并关闭.
			m_client_stream.reset();
			close();
		}
	}

	// on_upstream_closed 处理上游连接异常/关闭.
	void tun_tcp_flow::on_upstream_closed()
	{
		if (m_closed)
			return;

		m_client_stream.reset();
		close();
	}

	void tun_tcp_flow::close()
	{
		if (m_closed)
			return;

		m_closed = true;

		boost::system::error_code ec;
		net_tcp_socket(m_upstream).close(ec);
		m_client_stream.close();

		if (m_owner)
			m_owner->remove_tcp_flow(m_key, this);
	}

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
			udp_idle_timeout(m_option) + 30));
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

		// DoH 模式无需后端连接（每个查询独立发起 DoH 请求）.
		if (!m_doh_mode && !m_doh_via_proxy)
		{
			// 直连或 SOCKS5 需要本地 UDP 后端 socket.
			if (!m_connect_udp && !co_await open_backend())
			{
				close();
				co_return;
			}

			if (m_proxy && !co_await establish_proxy())
			{
				close();
				co_return;
			}

			if (m_connect_udp)
			{
				// HTTP CONNECT-UDP：启动串行发送协程与 capsule 接收循环.
				start_data_loops();
			}
			else
			{
				// 直连/SOCKS5：启动后端应答接收循环.
				net::co_spawn(m_executor,
					[this, self]() -> net::awaitable<void>
					{
						return recv_loop();
					}, net::detached);
			}
		}

		// 后端就绪，补发等待期间缓存的客户端数据.
		m_ready = true;
		for (auto& p : m_pending)
		{
			bool ok = false;
			if (m_connect_udp)
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
						XLOG_DBG << "tun udp dns query: " << qname << " type "
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
						XLOG_DBG << "tun udp dns cache hit: " << qname;
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
							XLOG_DBG << "tun udp dns cache put: " << key;
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
		if (m_connect_udp)
		{
			// HTTP CONNECT-UDP：封装 DATAGRAM capsule 写入隧道.
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
		auto header = build_socks5_udp_header(m_target);

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
			if (auto dns = pick_dns_server(m_option.dns_domestic_, m_target))
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
			// DoH 仅对 http(s) 代理生效（CONNECT 隧道为 HTTP 协议）；
			// socks 上游回退为原始查询经 socks 转发.
			if (!m_option.dns_doh_.empty() &&
				scheme.starts_with("http"))
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
				if (auto dns = pick_dns_server(
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
			if (auto dns = pick_dns_server(
				m_option.dns_domestic_, m_target))
				m_target = *dns;
		}
	}

	// resolve_udp_transport 判定 UDP 传输模式：http(s) 代理走 RFC 9298
	// CONNECT-UDP 隧道，socks 代理走 UDP ASSOCIATE，其余 scheme 不支持.
	bool tun_udp_flow::resolve_udp_transport()
	{
		// HTTP 代理通过 RFC 9298 CONNECT-UDP 隧道转发，无需本地 UDP 后端 socket.
		auto scheme = boost::to_lower_copy(
			std::string((*m_option.proxy_pass_).scheme()));
		m_connect_udp = scheme.starts_with("http");
		m_proto.store(static_cast<uint8_t>(m_connect_udp ?
			tun_conn_proto::connect_udp : tun_conn_proto::socks5));
		if (!scheme.starts_with("http") && !scheme.starts_with("socks"))
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

		// HTTP 代理：RFC 9298 CONNECT-UDP 隧道；SOCKS5：UDP ASSOCIATE.
		co_return m_connect_udp ?
			co_await do_connect_udp(std::move(sock)) :
			co_await do_socks5_associate(std::move(sock));
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
			std::string proxy_host(proxy_url.encoded_host());
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
		opt.target_host = "0.0.0.0";
		opt.target_port = 21;
		opt.proxy_hostname = true;
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
			auto& tcp_sock = boost::variant2::get<proxy_tcp_socket>(*m_control);
			auto remote = tcp_sock.lowest_layer().remote_endpoint(ec);
			if (!ec)
				m_backend_endp.address(remote.address());
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
		std::string proxy_host(proxy_url.encoded_host());

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

	// send_connect_udp_request 发送 RFC 9298 CONNECT-UDP 请求（absolute-form URI）.
	net::awaitable<bool> tun_udp_flow::send_connect_udp_request()
	{
		boost::system::error_code ec;

		const auto& proxy_url = *m_option.proxy_pass_;
		auto req = build_connect_udp_request(proxy_url, m_target);

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

		beast::flat_buffer resp_buf;
		http::response_parser<http::empty_body> resp_parser;
		resp_parser.skip(true);

		co_await http::async_read_header(
			http_sock, resp_buf, resp_parser, net_awaitable[ec]);
		if (ec)
		{
			XLOG_WARN << "tun udp read connect-udp response: " << ec.message();
			co_return false;
		}

		auto resp = resp_parser.release();
		if (resp.result() != http::status::switching_protocols)
		{
			XLOG_WARN << "tun udp connect-udp rejected: "
				<< static_cast<int>(resp.result());
			co_return false;
		}

		XLOG_DBG << "tun udp connect-udp established to target: " << m_target;
		co_return true;
	}

	// start_data_loops 启动串行发送协程与 capsule 接收循环.
	void tun_udp_flow::start_data_loops()
	{
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
		uint8_t buf[65536];
		size_t pos = 0;

		XLOG_DBG << "tun udp tx capsule to " << m_target.address().to_string()
			<< ":" << m_target.port() << " len=" << len;

		pos += varint_int_encode(udp_proxy_capsule_type, buf + pos);
		pos += varint_int_encode(1 + len, buf + pos);
		pos += varint_int_encode(0, buf + pos);

		if (pos + len > sizeof(buf))
		{
			close();
			return;
		}

		std::memcpy(buf + pos, data, len);
		pos += len;

		{
			std::lock_guard<std::mutex> lk(m_tx_mutex);
			// 发送协程消费速度跟不上时丢弃新包, 防止队列无界增长.
			if (m_tx_queue.size() >= k_max_udp_pending)
				return;
			m_tx_queue.emplace_back(buf, buf + pos);
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
			co_await net::async_write(*m_control, net::buffer(item), net_awaitable[ec]);
			if (ec)
				break;
		}

		close();
	}

	net::awaitable<void> tun_udp_flow::recv_connect_udp_loop()
	{
		if (!m_control)
			co_return;

		for (; !m_closed;)
		{
			boost::system::error_code ec;
			auto [capsule_type, capsule_value] = co_await read_capsule(ec);
			if (ec)
				break;

			XLOG_DBG << "tun udp rx capsule type=" << capsule_type
				<< " len=" << capsule_value.size();

			// 仅处理 DATAGRAM capsule（RFC 9297）.
			if (capsule_type != udp_proxy_capsule_type ||
				capsule_value.empty())
				continue;

			// 解析 context ID（本项目固定使用 0）.
			// 使用带边界的解码, 防止畸形 capsule 导致越界读取.
			auto val_data = reinterpret_cast<const uint8_t*>(capsule_value.data());
			auto [ctx_id_len, ctx_id] = varint_int_decode_bounded(
				val_data, capsule_value.size());
			if (ctx_id_len == 0 || ctx_id != 0)
				continue;

			size_t udp_len = capsule_value.size() - ctx_id_len;
			if (udp_len == 0)
				continue;

			touch();
			co_await reply(capsule_value.data() + ctx_id_len, udp_len);
		}

		close();
	}

	net::awaitable<std::pair<uint64_t, std::vector<char>>>
	tun_udp_flow::read_capsule(boost::system::error_code& ec)
	{
		// 读取 capsule type 与 length（varint）.
		auto capsule_type = co_await read_varint_from_stream(*m_control, ec);
		if (ec)
			co_return std::pair<uint64_t, std::vector<char>>{};

		auto capsule_length = co_await read_varint_from_stream(*m_control, ec);
		if (ec)
			co_return std::pair<uint64_t, std::vector<char>>{};

		if (capsule_length > 65535)
		{
			XLOG_WARN << "tun udp capsule too large: " << capsule_length;
			ec = net::error::invalid_argument;
			co_return std::pair<uint64_t, std::vector<char>>{};
		}

		std::vector<char> capsule_value(
			static_cast<size_t>(capsule_length), '\0');
		if (capsule_length > 0)
		{
			co_await net::async_read(
				*m_control, net::buffer(capsule_value), net_awaitable[ec]);
			if (ec)
				co_return std::pair<uint64_t, std::vector<char>>{};
		}

		co_return std::make_pair(capsule_type, std::move(capsule_value));
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
			size_t header_size = socks5_udp_header_size(buf, n);
			if (n <= header_size)
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
		XLOG_DBG << "tun udp dns cache put: " << key;
	}

	void tun_udp_flow::touch()
	{
		if (m_closed || !m_expire)
			return;

		// 每次活动重置超时计时，超时后关闭会话.
		m_expire->expires_after(std::chrono::seconds(udp_idle_timeout(m_option)));
		m_expire->async_wait(
			[this](const boost::system::error_code& ec)
			{
				if (ec)
					return;  // 计时器被取消（新活动重置）.
				close();
			});
	}

	//////////////////////////////////////////////////////////////////////////
	// tun_server

	tun_server::tun_server(net::any_io_executor executor, proxy_server_option opt)
		: m_ioc(static_cast<net::io_context&>(executor.context()))
		, m_executor(std::move(executor))
		, m_option(std::move(opt))
		, m_tunio(std::make_unique<tunio::tunio>(m_ioc))
	{}

	std::shared_ptr<tun_server>
	tun_server::make(net::any_io_executor executor, proxy_server_option opt)
	{
		return std::shared_ptr<tun_server>(
			new tun_server(std::move(executor), std::move(opt)));
	}

	tun_server::~tun_server()
	{
		close();
	}

	net::awaitable<boost::system::result<bool>>
	tun_server::make_ssl_socket(tcp::socket& remote_socket,
		std::string_view sni, std::optional<variant_stream_type>& ssl_sock)
	{
		boost::system::error_code ec;

		if (!m_ssl_client_context)
		{
			m_ssl_client_context.emplace(net::ssl::context::sslv23_client);

			// 使用通用函数配置 SSL context（验证模式、CA 证书、主机名验证）.
			ec = configure_ssl_client_ctx(*m_ssl_client_context,
				m_option.disable_check_cert_,
				std::string(sni),
				m_option.ssl_cacert_path_);
			if (ec)
			{
				// 配置失败，重置 context，使下次调用重新初始化.
				m_ssl_client_context.reset();
				co_return ec;
			}
		}

		// 初始化为 SSL 加密的 TCP 流.
		ssl_sock.emplace(init_proxy_stream(std::move(remote_socket), *m_ssl_client_context));
		auto& ssl_socket = boost::variant2::get<ssl_tcp_stream>(*ssl_sock);

		// 设置 SNI 主机名以兼容需要 SNI 的服务器.
		SSL_set_tlsext_host_name(ssl_socket.native_handle(), sni.data());

		// 进行 SSL 握手.
		co_await ssl_socket.async_handshake(
			net::ssl::stream_base::client, net_awaitable[ec]);
		if (ec)
			co_return ec;

		co_return true;
	}

	bool tun_server::open_engine() noexcept
	{
		tunio::tun_config cfg;
		cfg.dev_name = m_option.tun_name_;
		cfg.mtu = m_option.tun_mtu_ > 0 ? m_option.tun_mtu_ : 1500;
		cfg.udp_idle_timeout = std::chrono::seconds(udp_idle_timeout(m_option));

		// 地址/路由由外部脚本或 VpnService 配置，引擎只创建设备；
		// 但环路防护需要本地地址：Linux 下从设备查询实际 IP 供 tunio
		// 丢弃以本地地址为源的回环入包（其余平台由外部配置，留空则不启用）.
#if defined(__linux__)
		query_tun_device_ip(m_option.tun_name_, cfg.ipv4_addr);
#else
		cfg.ipv4_addr.clear();
#endif
		cfg.netmask.clear();

		// Android VpnService 场景: 注入外部已打开的 fd.
		if (m_option.tun_fd_ >= 0)
		{
#if defined(_WIN32)
			// Windows 下 native_handle_type 为 HANDLE(void*), 外部注入
			// 的句柄经 int 传递后按整型宽度转换.
			cfg.external_handle = reinterpret_cast<tunio::native_handle_type>(
				static_cast<intptr_t>(m_option.tun_fd_));
#else
			cfg.external_handle = m_option.tun_fd_;
#endif
			cfg.external_mtu = cfg.mtu;
		}

		boost::system::error_code ec;

#if defined(_WIN32)
		// Windows 下 tunio 使用 wintun 驱动: 未安装时先从 exe 资源解压安装.
		if (!ensure_wintun_driver())
		{
			XLOG_ERR << "tun open engine failed: wintun driver unavailable";
			return false;
		}
#endif
		if (!m_tunio->open(cfg, ec))
		{
			XLOG_ERR << "tun open engine failed: " << ec.message();
			return false;
		}

		m_tcp_acceptor = std::make_unique<tunio::tun_acceptor>(*m_tunio);
		m_udp_acceptor = std::make_unique<tunio::tun_udp_acceptor>(*m_tunio);

		return true;
	}

	void tun_server::start_accept_loops() noexcept
	{
		m_running = true;

		auto self = shared_from_this();
		net::co_spawn(m_executor,
			[this, self]() -> net::awaitable<void>
			{
				return run_accept_tcp();
			}, net::detached);
		net::co_spawn(m_executor,
			[this, self]() -> net::awaitable<void>
			{
				return run_accept_udp();
			}, net::detached);
	}

	net::awaitable<void> tun_server::run_accept_tcp()
	{
		auto self = shared_from_this();

		for (; !m_abort;)
		{
			tunio::tun_stream peer(m_executor);
			boost::system::error_code ec;
			co_await m_tcp_acceptor->async_accept(peer, net_awaitable[ec]);
			if (ec)
				break;
			if (m_abort)
				break;

			const auto remote = peer.remote_endpoint();
			const auto target = peer.original_destination();

#if defined(__linux__)
			// 环路防护：客户端地址等于 tun 设备自身地址说明该连接是
			// 未绕过 tun 的出站连接回环（Android protect 失败 / Linux
			// 未配置 SO_MARK 策略路由），若继续转发将形成连接风暴.
			if (is_tun_loopback_source(remote.address(),
				m_option.tun_name_))
			{
				XLOG_WARN << "tun loopback connection rejected: "
					<< remote.address().to_string() << ":"
					<< remote.port() << " -> "
					<< target.address().to_string() << ":"
					<< target.port();
				peer.reset();
				continue;
			}
#endif

			tcp_flow_key key{
				remote.address(), remote.port(),
				target.address(), target.port()
			};

			{
				std::lock_guard<std::mutex> lk(m_flows_mutex);
				// 并发 flow 超限: 拒绝新连接, 防止流量风暴耗尽 fd.
				if (m_tcp_flows.size() >= k_max_tcp_flows)
					continue;

				auto flow = std::make_shared<tun_tcp_flow>(
					m_executor, self, m_option, key, std::move(peer));
				auto [it, inserted] = m_tcp_flows.emplace(key, flow);
				if (inserted)
				{
					flow->start();
					++m_conn_total;
				}
				else
				{
					// 键冲突（理论不可达）：关闭新连接.
					flow->close();
				}
			}
		}

		co_return;
	}

	net::awaitable<void> tun_server::run_accept_udp()
	{
		auto self = shared_from_this();

		for (; !m_abort;)
		{
			tunio::tun_udp_socket peer(m_executor);
			boost::system::error_code ec;
			co_await m_udp_acceptor->async_accept(peer, net_awaitable[ec]);
			if (ec)
				break;
			if (m_abort)
				break;

#if defined(__linux__)
			// 环路防护（与 TCP 一致）：客户端地址等于 tun 设备自身地址的
			// UDP 会话是未绕过 tun 的出站流量回环，拒绝避免形成风暴.
			if (is_tun_loopback_source(
				peer.client_endpoint().address(), m_option.tun_name_))
			{
				XLOG_WARN << "tun loopback udp session rejected: "
					<< peer.client_endpoint().address().to_string()
					<< ":" << peer.client_endpoint().port();
				continue;
			}
#endif

			// 并发 flow 超限: 关闭会话, 防止流量风暴耗尽 fd.
			{
				std::lock_guard<std::mutex> lk(m_flows_mutex);
				if (m_udp_flows.size() >= k_max_udp_flows)
					continue;
			}

			auto flow = std::make_shared<tun_udp_flow>(
				m_executor, self, m_option, std::move(peer));
			flow->start();
		}

		co_return;
	}

	void tun_server::start() noexcept
	{
		if (m_abort)
			return;

		// 预初始化上游代理 SSL context: 多协程（TCP/UDP 流）并发建连时
		// 若各自惰性初始化会产生数据竞争, 导致部分 SSL 握手挂起.
		if (!m_ssl_client_context && m_option.proxy_pass_ &&
			proxy_use_ssl(*m_option.proxy_pass_, m_option))
		{
			boost::system::error_code ec;
			std::string proxy_host((*m_option.proxy_pass_).encoded_host());
			auto sni = m_option.proxy_ssl_name_.empty() ?
				proxy_host : m_option.proxy_ssl_name_;
			m_ssl_client_context.emplace(net::ssl::context::sslv23_client);
			ec = configure_ssl_client_ctx(*m_ssl_client_context,
				m_option.disable_check_cert_, sni, m_option.ssl_cacert_path_);
			if (ec)
			{
				XLOG_WARN << "tun init ssl client context: " << ec.message();
				m_ssl_client_context.reset();
			}
		}

		// 等待外部注入 TUN fd 模式（Android VpnService）：不创建设备,
		// 由 launcher 控制通道 set_tun_fd 注入后启动 accept 循环.
		if (m_option.tun_wait_fd_)
			return;

		// 预建 proxy_pass 连接池（TCP flow 走代理时复用已建立的连接）.
		if (m_option.proxy_pass_ && m_option.proxy_pass_pool_size_ > 0)
			proxy_conn_pool();

		if (!open_engine())
			return;

		XLOG_INFO << "tun device opened, mtu: " << m_tunio->mtu();

		start_accept_loops();
	}

	void tun_server::set_tun_fd(int fd) noexcept
	{
		if (m_abort)
			return;

		// 关闭现有 flow：其流对象绑定旧引擎，重开后全部失效.
		close_flows();

		// 关闭旧引擎（取消挂起 accept，旧 accept 循环随之退出）.
		m_tunio->close();

		// 记录注入 fd，open_engine 以外部句柄模式打开新设备.
		m_option.tun_fd_ = fd;

		if (!open_engine())
			return;

		XLOG_INFO << "tun device injected: fd=" << fd
			<< ", mtu: " << m_tunio->mtu();

		// 重启 accept 循环.
		start_accept_loops();

		// 预建 proxy_pass 连接池（TCP flow 走代理时复用已建立的连接）.
		if (m_option.proxy_pass_ && m_option.proxy_pass_pool_size_ > 0)
			proxy_conn_pool();
	}

	void tun_server::set_protect_handler(
		std::function<net::awaitable<bool>(int)> handler)
	{
		m_protect_handler = std::move(handler);
	}

	void tun_server::set_dns_cache(dns_response_cache* cache) noexcept
	{
		m_dns_cache = cache;
		XLOG_INFO << "tun dns cache "
			<< (cache ? "enabled" : "disabled");
	}

	dns_response_cache* tun_server::dns_cache() const noexcept
	{
		return m_dns_cache;
	}

	net::awaitable<bool> tun_server::protect_socket(int fd)
	{
		if (m_protect_handler)
			co_return co_await m_protect_handler(fd);
		co_return true;
	}

	void tun_server::close_flows() noexcept
	{
		// 锁内搬出 flow，锁外逐个关闭，避免 flow 析构时
		// 反向调用 remove_*_flow 再次加锁造成死锁.
		std::vector<std::shared_ptr<tun_tcp_flow>> tcp_flows;
		std::vector<std::shared_ptr<tun_udp_flow>> udp_flows;
		{
			std::lock_guard<std::mutex> lk(m_flows_mutex);
			tcp_flows.reserve(m_tcp_flows.size());
			for (auto& [key, flow] : m_tcp_flows)
				tcp_flows.push_back(flow);
			udp_flows.reserve(m_udp_flows.size());
			for (auto& [key, flow] : m_udp_flows)
				udp_flows.push_back(flow);
			m_tcp_flows.clear();
			m_udp_flows.clear();
		}

		for (auto& flow : tcp_flows)
			flow->close();
		for (auto& flow : udp_flows)
			flow->close();
	}

	void tun_server::close() noexcept
	{
		auto impl = [this]()
		{
			m_abort = true;
			m_running = false;

			close_flows();

			// 关闭 DoH 连接池（唤醒所有挂起的 DNS 查询）.
			if (m_doh_client)
				m_doh_client->close();

			// 关闭 proxy_pass 预选连接池（释放所有空闲连接）.
			if (m_proxy_pool)
				m_proxy_pool->close();

			if (m_tunio)
				m_tunio->close();
		};

		// close() 可能由非 io_context 线程调用（Android 上 stop 常由
		// 服务工作线程触发）, 而 flows/DoH 连接池/proxy 连接池/tunio 引擎
		// 都在 io_context 线程上被数据通路协程访问: 直接在本线程关闭会与
		// 数据通路产生数据竞争（实测 doh_client 连接池并发释放导致
		// scudo double free）. 投递到 io_context 串行执行后再返回.
		if (m_ioc.stopped())
		{
			// io_context 已停止（析构路径）: 无并发协程, 直接清理.
			impl();
			return;
		}
		std::promise<void> done;
		net::dispatch(m_executor,
			[impl, &done]() mutable
			{
				impl();
				done.set_value();
			});
		done.get_future().wait();
	}

	tun_server::stats tun_server::get_stats() noexcept
	{
		stats st;
		st.rx_bytes = m_rx_bytes.load();
		st.tx_bytes = m_tx_bytes.load();
		st.conn_total = m_conn_total.load();
		st.pool_connections = m_proxy_pool ? m_proxy_pool->idle_count() : 0;
		{
			std::lock_guard<std::mutex> lk(m_flows_mutex);
			size_t active = m_tcp_flows.size();
			// DNS 查询流（目标 53 端口）为内部基础设施流量, 不纳入活跃连接.
			for (auto& [key, flow] : m_udp_flows)
			{
				if (flow->m_target.port() == 53)
					continue;
				++active;
			}
			st.active_connections = active;
		}
		return st;
	}

	std::vector<tun_server::conn_info> tun_server::connections() noexcept
	{
		std::vector<conn_info> out;

		std::vector<std::shared_ptr<tun_tcp_flow>> tcp_flows;
		std::vector<std::shared_ptr<tun_udp_flow>> udp_flows;
		{
			std::lock_guard<std::mutex> lk(m_flows_mutex);
			tcp_flows.reserve(m_tcp_flows.size());
			for (auto& [key, flow] : m_tcp_flows)
				tcp_flows.push_back(flow);
			udp_flows.reserve(m_udp_flows.size());
			for (auto& [key, flow] : m_udp_flows)
				udp_flows.push_back(flow);
		}

		const auto now = std::chrono::steady_clock::now();
		out.reserve(tcp_flows.size() + udp_flows.size());

		auto append = [&](uint64_t id, const std::string& client,
			const std::string& target, uint8_t proto,
			const std::chrono::steady_clock::time_point& started,
			uint64_t rx, uint64_t tx)
		{
			conn_info c;
			c.id = id;
			c.client_ip = client;
			c.target = target;
			switch (static_cast<tun_conn_proto>(proto))
			{
			case tun_conn_proto::http:
				c.proto = "http";
				break;
			case tun_conn_proto::https:
				c.proto = "https";
				break;
			case tun_conn_proto::socks5:
				c.proto = "socks5";
				break;
			case tun_conn_proto::connect_udp:
				c.proto = "connect-udp";
				break;
			case tun_conn_proto::doh_dns:
				c.proto = "doh-dns";
				break;
			case tun_conn_proto::udp:
				c.proto = "udp";
				break;
			default:
				c.proto = "tcp";
				break;
			}
			auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
				now - started).count();
			c.elapsed = elapsed < 0 ? 0 : static_cast<int64_t>(elapsed);
			c.rx_bytes = rx;
			c.tx_bytes = tx;
			out.emplace_back(std::move(c));
		};

		for (auto& flow : tcp_flows)
		{
			std::string client = flow->m_client.address().to_string() +
				":" + std::to_string(flow->m_client.port());
			std::string target = flow->m_target.address().to_string() +
				":" + std::to_string(flow->m_target.port());
			append(flow->m_conn_id, client, target,
				flow->m_proto.load(), flow->m_started,
				flow->m_rx_bytes.load(), flow->m_tx_bytes.load());
		}

		for (auto& flow : udp_flows)
		{
			// DNS 查询流（目标 53 端口）不列入会话明细, 避免大量
			// 查询刷屏会话列表.
			if (flow->m_target.port() == 53)
				continue;
			std::string client = flow->m_client.address().to_string() +
				":" + std::to_string(flow->m_client.port());
			std::string target = flow->m_target.address().to_string() +
				":" + std::to_string(flow->m_target.port());
			append(flow->m_conn_id, client, target,
				flow->m_proto.load(), flow->m_started,
				flow->m_rx_bytes.load(), flow->m_tx_bytes.load());
		}

		return out;
	}

	uint64_t tun_server::next_conn_id() noexcept
	{
		return m_conn_seq.fetch_add(1, std::memory_order_relaxed) + 1;
	}

	void tun_server::register_udp_flow(const tcp_flow_key& key,
		const std::shared_ptr<tun_udp_flow>& flow)
	{
		std::lock_guard<std::mutex> lk(m_flows_mutex);
		m_udp_flows.emplace(key, flow);
	}

	void tun_server::remove_tcp_flow(const tcp_flow_key& key,
		const tun_tcp_flow* flow)
	{
		std::lock_guard<std::mutex> lk(m_flows_mutex);
		auto it = m_tcp_flows.find(key);
		if (it != m_tcp_flows.end() && it->second.get() == flow)
			m_tcp_flows.erase(it);
	}

	void tun_server::remove_udp_flow(const tcp_flow_key& key,
		const tun_udp_flow* flow)
	{
		std::lock_guard<std::mutex> lk(m_flows_mutex);
		auto it = m_udp_flows.find(key);
		if (it != m_udp_flows.end() && it->second.get() == flow)
			m_udp_flows.erase(it);
	}

	bool tun_server::cidr_match(const net::ip::address& addr) const noexcept
	{
		boost::system::error_code ec;

		for (const auto& ip_cidr : m_option.proxy_cidr_)
		{
			if (ip_cidr.empty())
				continue;

			try
			{
				auto iponly = net::ip::make_address(ip_cidr, ec);
				if (!ec)
				{
					if (iponly == addr)
						return true;
					continue;
				}

				ec.clear();
				auto netaddr4 = net::ip::make_network_v4(ip_cidr, ec);
				if (!ec)
				{
					if ((addr.to_v4().to_uint() & netaddr4.netmask().to_uint()) ==
						netaddr4.network().to_uint())
						return true;
					continue;
				}

				ec.clear();
				auto netaddr6 = net::ip::make_network_v6(ip_cidr, ec);
				if (!ec)
				{
					auto net6 = net::ip::make_network_v6(addr.to_v6(),
						netaddr6.prefix_length());
					if (net6.canonical() == netaddr6.canonical())
						return true;
				}
			}
			catch (const std::exception&)
			{}
		}

		return false;
	}

	bool tun_server::domain_match(const std::string& domain) const noexcept
	{
		for (const auto& d : m_option.proxy_domains_)
		{
			if (d.empty() || domain.size() < d.size())
				continue;

			if (domain == d)
				return true;

			if (domain.size() > d.size() &&
				domain.ends_with(d) &&
				domain[domain.size() - d.size() - 1] == '.')
				return true;
		}

		return false;
	}

	void tun_server::record_dns_answer(const char* data, size_t len) noexcept
	{
		std::string qname;
		std::vector<std::pair<net::ip::address, uint32_t>> answers;
		if (!parse_dns_response(data, len, qname, answers))
			return;

		// 仅记录命中代理域名列表的解析结果.
		if (!domain_match(qname))
			return;

		auto now = std::chrono::steady_clock::now();
		std::vector<dns_ip_entry> entries;
		entries.reserve(answers.size());
		for (const auto& [ip, ttl] : answers)
		{
			auto expire = now + std::chrono::seconds(ttl > 0 ? ttl : 60);
			entries.push_back({ ip, expire });
		}

		{
			std::lock_guard<std::mutex> lk(m_domain_ips_mutex);
			m_domain_ips[qname] = std::move(entries);
		}
	}

	bool tun_server::ip_match_proxy(const net::ip::address& addr) const noexcept
	{
		// 配置了上游代理但未指定任何分流规则时，默认全部走代理.
		if (m_option.proxy_pass_ &&
			m_option.proxy_cidr_.empty() &&
			m_option.proxy_domains_.empty())
			return true;

		if (cidr_match(addr))
			return true;

		// 域名解析缓存：目标 IP 命中且对应域名在代理表时走代理.
		std::lock_guard<std::mutex> lk(m_domain_ips_mutex);

		auto now = std::chrono::steady_clock::now();
		for (auto it = m_domain_ips.begin(); it != m_domain_ips.end();)
		{
			auto& entries = it->second;

			// 清理过期条目.
			entries.erase(std::remove_if(entries.begin(), entries.end(),
				[now](const dns_ip_entry& e) { return e.expire <= now; }),
				entries.end());

			if (entries.empty())
			{
				it = m_domain_ips.erase(it);
				continue;
			}

			for (const auto& e : entries)
			{
				if (e.ip == addr)
					return true;
			}

			++it;
		}

		return false;
	}

#else // 不支持的平台

	std::shared_ptr<tun_server>
	tun_server::make(net::any_io_executor executor, proxy_server_option opt)
	{
		(void)executor;
		(void)opt;
		return nullptr;
	}

#endif // defined(__linux__) || defined(__APPLE__) || defined(_WIN32)

} // namespace proxy
