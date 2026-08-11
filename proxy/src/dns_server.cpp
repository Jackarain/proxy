//
// dns_server.cpp
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/dns_server.hpp"
#include "proxy/proxy_util.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <openssl/ssl.h>

#include <array>
#include <cstdint>
#include <memory>


namespace proxy {

	// DNS 记录类型常量.
	static constexpr uint16_t DNS_TYPE_A = 1;
	static constexpr uint16_t DNS_TYPE_AAAA = 28;

	// UDP DNS 在途查询协程数上限，超过直接丢弃（UDP 无连接，丢包由客户端
	// 自行重试）.
	inline constexpr int k_dns_max_inflight = 256;

	// 本地解析应答的 TTL（系统解析不暴露原始 TTL，统一使用该值）.
	static constexpr uint32_t dns_local_ttl = 60;

	//////////////////////////////////////////////////////////////////////////

	dns_server::dns_server(
		net::any_io_executor executor,
		net::io_context& backend_context,
		bool scheduler_locking,
		proxy_server_option option)
		: m_executor(std::move(executor))
		, m_backend_context(backend_context)
		, m_scheduler_locking(scheduler_locking)
		, m_option(std::move(option))
	{
		rebuild_cache();
	}

	void dns_server::start() noexcept
	{
		if (m_option.dns_udp_port_ <= 0)
			return;
		start_listen();
	}

	void dns_server::close() noexcept
	{
		m_abort = true;
		stop_listen();
	}

	std::string dns_server::apply_options(const proxy_server_option& opt)
	{
		// 缓存参数变化时才重建缓存：launcher 每次 set_config 都携带完整配置，
		// 若每次都重建会频繁清空 DNS 缓存，降低缓存命中率.
		bool cache_changed =
			opt.dns_cache_size_ != m_option.dns_cache_size_ ||
			opt.dns_cache_ttl_ != m_option.dns_cache_ttl_;

		bool was_listening = m_udp_socket != nullptr;
		int old_port = m_option.dns_udp_port_;
		int new_port = opt.dns_udp_port_;

		m_option = opt;
		if (cache_changed)
			rebuild_cache();

		// 端口热改：启动 / 停止 / 切换监听.
		if (new_port > 0 && !was_listening)
		{
			start_listen();
		}
		else if (new_port <= 0 && was_listening)
		{
			stop_listen();
		}
		else if (new_port > 0 && was_listening && new_port != old_port)
		{
			stop_listen();
			start_listen();
		}

		return {};
	}

	dns_response_cache* dns_server::cache() noexcept
	{
		return m_cache.get();
	}

	bool dns_server::no_ipv6() const noexcept
	{
		return m_option.dns_no_ipv6_;
	}

	// rebuild_cache 根据当前配置重建缓存（size/ttl 变化时清空重建）.
	void dns_server::rebuild_cache() noexcept
	{
		if (m_option.dns_cache_size_ > 0 && m_option.dns_cache_ttl_ > 0)
			m_cache = std::make_unique<dns_response_cache>(
				m_option.dns_cache_size_, m_option.dns_cache_ttl_);
		else
			m_cache.reset();
	}

	// start_listen 创建 UDP socket 并绑定 dns_udp_port_ 端口.
	void dns_server::start_listen() noexcept
	{
		if (m_udp_socket)
			return;

		boost::system::error_code ec;

		auto sock = std::make_shared<udp::socket>(m_executor);
		sock->open(udp::v4(), ec);
		if (ec)
		{
			XLOG_ERR << "udp dns open failed: " << ec.message();
			return;
		}

		sock->set_option(udp::socket::reuse_address(true), ec);

		sock->bind(udp::endpoint(udp::v4(), m_option.dns_udp_port_), ec);
		if (ec)
		{
			XLOG_ERR << "udp dns bind port " << m_option.dns_udp_port_
				<< " failed: " << ec.message();
			return;
		}

		m_udp_socket = sock;
		XLOG_INFO << "udp dns listening on " << sock->local_endpoint(ec);

		net::co_spawn(m_executor, udp_listen(sock), net::detached);
	}

	// stop_listen 关闭当前 UDP 监听 socket，使监听协程退出.
	void dns_server::stop_listen() noexcept
	{
		if (auto sock = m_udp_socket)
		{
			// 立即解除对监听 socket 的引用，使随后调用的 start_listen() 能
			// 创建新的监听 socket。旧 socket 由监听协程通过 shared_ptr 持有，
			// 关闭后协程立即退出并释放，不会悬挂.
			m_udp_socket.reset();
			boost::system::error_code ec;
			sock->close(ec);
		}
	}

	// udp_listen UDP DNS 请求接收主循环.
	net::awaitable<void> dns_server::udp_listen(std::shared_ptr<udp::socket> sock)
	{
		boost::system::error_code ec;

		while (!m_abort)
		{
			std::array<char, 4096> recv_buf{};
			udp::endpoint peer;

			auto recv_len = co_await sock->async_receive_from(
				net::buffer(recv_buf), peer, net_awaitable[ec]);
			if (ec)
				break;  // socket 被关闭（close/热改），退出.

			if (recv_len == 0)
				continue;

			// 复制查询报文，避免并发协程共享读缓冲.
			std::string query(recv_buf.data(), recv_len);

			// 有界并发：限制在途查询协程数量，超出直接丢弃.
			if (m_inflight.fetch_add(1) >= k_dns_max_inflight)
			{
				m_inflight.fetch_sub(1);
				XLOG_WARN << "udp dns: in-flight queries full ("
					<< k_dns_max_inflight << "), dropping packet from " << peer;
				continue;
			}

			net::co_spawn(m_executor,
				[this, sock, peer, query = std::move(query)]() -> net::awaitable<void>
				{
					// 结束时递减在途计数.
					struct inflight_guard
					{
						std::atomic<int>& counter;
						~inflight_guard() { counter.fetch_sub(1); }
					} guard{ m_inflight };

					co_await handle_query(sock, peer, std::move(query));
				}, net::detached);
		}

		// 协程退出时清理（仅当仍是当前监听 socket）.
		if (m_udp_socket == sock)
			m_udp_socket.reset();

		co_return;
	}

	// handle_query 处理单个 UDP DNS 请求：配置了 dns_upstream 时转发到上游，
	// 否则按系统默认解析流程构造响应.
	net::awaitable<void> dns_server::handle_query(
		const std::shared_ptr<udp::socket>& sock,
		const udp::endpoint& peer, std::string query)
	{
		boost::system::error_code ec;

		std::string qname;
		uint16_t qtype = DNS_TYPE_A;
		bool cd = false;
		bool do_flag = false;
		proxy_session::dns_parse_query(query, qname, qtype);
		proxy_session::dns_query_flags(query, cd, do_flag);

		// 禁用 IPv6 解析返回：AAAA 查询直接返回空应答（NODATA），不转发上游.
		if (m_option.dns_no_ipv6_ && qtype == DNS_TYPE_AAAA)
		{
			auto resp = proxy_session::dns_build_response(query, 0, {});
			if (!resp.empty())
			{
				co_await sock->async_send_to(
					net::buffer(resp), peer, net_awaitable[ec]);
				if (ec)
				{
					XLOG_WARN << "udp dns write response error: " << ec.message();
					co_return;
				}
			}
			XLOG_DBG << "udp dns query: " << qname << " type "
				<< proxy_session::dns_type_to_string(qtype)
				<< " from " << peer << ", ipv6 disabled, return empty";
			co_return;
		}

		// 缓存键（域名 + 类型 + CD/DO 标志）.
		std::string cache_key;
		if (m_cache && !qname.empty())
			cache_key = proxy_session::dns_cache_key(qname, qtype, cd, do_flag);

		// 缓存命中：改写事务 ID 后直接回包.
		if (m_cache && !cache_key.empty())
		{
			if (auto hit = m_cache->get(cache_key); hit)
			{
				uint16_t qid = static_cast<uint16_t>(
					(static_cast<uint8_t>(query[0]) << 8) |
					static_cast<uint8_t>(query[1]));
				auto resp = proxy_session::dns_set_id(*hit, qid);
				co_await sock->async_send_to(
					net::buffer(resp), peer, net_awaitable[ec]);
				if (ec)
				{
					XLOG_WARN << "udp dns write response error: " << ec.message();
					co_return;
				}
				XLOG_DBG << "udp dns query: " << qname << " type "
					<< proxy_session::dns_type_to_string(qtype)
					<< " from " << peer << ", cache hit";
				co_return;
			}
		}

		// 转发上游或本地解析.
		std::string response;
		if (m_option.dns_upstream_ && !m_option.dns_upstream_->empty())
		{
			auto& upstream = *m_option.dns_upstream_;
			bool ok = false;
			if (boost::istarts_with(upstream, "https://"))
				ok = co_await doh_query_raw(query, response);
			else
				ok = co_await udp_query_raw(query, response);
			if (!ok)
				response = proxy_session::dns_build_response(query, 2, {}); // SERVFAIL
		}
		else
		{
			co_await resolve_normal(query, response);
		}

		if (response.empty())
			co_return;

		// 写入缓存（剥离事务 ID；SERVFAIL 是临时故障，不缓存）.
		if (m_cache && !cache_key.empty() &&
			proxy_session::dns_cacheable(response))
			m_cache->put(cache_key, proxy_session::dns_strip_id(response));

		co_await sock->async_send_to(
			net::buffer(response), peer, net_awaitable[ec]);
		if (ec)
		{
			XLOG_WARN << "udp dns write response error: " << ec.message();
			co_return;
		}

		XLOG_DBG << "udp dns query: " << qname << " type "
			<< proxy_session::dns_type_to_string(qtype)
			<< " from " << peer << ", done";

		co_return;
	}

	// udp_query_raw 通过 UDP 上游转发 DNS 查询.
	net::awaitable<bool> dns_server::udp_query_raw(
		const std::string& dns_query, std::string& output)
	{
		boost::system::error_code ec;

		auto& upstream = *m_option.dns_upstream_;
		auto colon_pos = upstream.find(':');
		if (colon_pos == std::string::npos)
			co_return false;

		auto dns_host = upstream.substr(0, colon_pos);
		int dns_port = 0;
		try
		{
			dns_port = std::stoi(upstream.substr(colon_pos + 1));
		}
		catch (const std::exception&)
		{
			co_return false;
		}

		auto dns_socket = std::make_shared<udp::socket>(
			co_await net::this_coro::executor);
		udp::endpoint dns_endpoint(
			net::ip::make_address(dns_host, ec), static_cast<uint16_t>(dns_port));
		if (ec)
			co_return false;

		dns_socket->open(dns_endpoint.protocol(), ec);
		if (ec)
			co_return false;

		co_await dns_socket->async_send_to(
			net::buffer(dns_query), dns_endpoint, net_awaitable[ec]);
		if (ec)
			co_return false;

		// 5 秒超时：上游无响应时关闭 socket，使接收失败返回，防止协程因
		// 上游不响应而永久挂起（占用在途查询计数）.
		net::steady_timer timer(co_await net::this_coro::executor);
		timer.expires_after(std::chrono::seconds(5));
		std::weak_ptr<udp::socket> weak_sock(dns_socket);
		timer.async_wait([weak_sock](const boost::system::error_code& tec) {
			if (!tec)
			{
				if (auto sock = weak_sock.lock())
				{
					boost::system::error_code close_ec;
					sock->close(close_ec);
				}
			}
		});

		// 65535 为 UDP 上 DNS 报文的最大长度：固定 4096 会把携带大 payload
		// （DNSSEC 常见）的响应截断.
		std::array<char, 65535> recv_buf{};
		udp::endpoint recv_endp;
		auto recv_len = co_await dns_socket->async_receive_from(
			net::buffer(recv_buf), recv_endp, net_awaitable[ec]);
		timer.cancel();

		if (ec)
			co_return false;

		dns_socket->close(ec);
		output.assign(recv_buf.data(), recv_len);
		co_return true;
	}

	// resolve_host 解析主机地址（在 backend 执行上下文执行同步解析）.
	net::awaitable<tcp::resolver::results_type>
	dns_server::resolve_host(const std::string& host, uint16_t port)
	{
		boost::system::error_code ec;

		auto ex = co_await backend_switch_to(
			m_scheduler_locking, m_backend_context, m_executor);

		tcp::resolver resolver{ ex };
		auto targets = co_await resolver.async_resolve(
			host, std::to_string(port), net_awaitable[ec]);

		co_await backend_switch_from(m_scheduler_locking, m_executor);

		if (ec)
		{
			XLOG_WARN << "dns server resolve: " << host
				<< ", error: " << ec.message();
			co_return tcp::resolver::results_type{};
		}

		co_return targets;
	}

	// doh_query_raw 通过 DoH (DNS over HTTPS) 上游转发 DNS 查询.
	net::awaitable<bool> dns_server::doh_query_raw(
		const std::string& dns_query, std::string& output)
	{
		boost::system::error_code ec;

		auto parsed = parse_urlinfo(*m_option.dns_upstream_);
		if (parsed.has_error())
			co_return false;

		auto [scheme, user, passwd, doh_host, doh_port, doh_path] = *parsed;
		if (doh_path.empty() || doh_path == "/")
			doh_path = "/dns-query";

		// 解析 DoH 服务器地址.
		tcp::resolver::results_type targets;
		if (!is_hostname(doh_host))
		{
			tcp::endpoint endp(
				net::ip::make_address(doh_host), doh_port);
			targets = tcp::resolver::results_type::create(
				endp, std::string(doh_host), "");
		}
		else
		{
			targets = co_await resolve_host(std::string(doh_host), doh_port);
		}

		if (targets.empty())
			co_return false;

		// 连接到 DoH 服务器.
		tcp::socket doh_socket(m_executor);
		ec = boost::asio::error::host_not_found;
		for (const auto& entry : targets)
		{
			auto endp = entry.endpoint();
			co_await doh_socket.async_connect(endp, net_awaitable[ec]);
			if (!ec)
				break;
		}
		if (ec)
			co_return false;

		// 创建 per-request SSL context, 确保每个 DoH 服务器使用正确的主机名校验.
		net::ssl::context doh_ssl_ctx(net::ssl::context::sslv23_client);
		ec = configure_ssl_client_ctx(doh_ssl_ctx,
			m_option.disable_check_cert_,
			std::string(doh_host));
		if (ec)
		{
			XLOG_WARN << "configure ssl context for doh: " << doh_host
				<< " error: " << ec.message();
			co_return false;
		}

		net::ssl::stream<tcp::socket> ssl_stream(std::move(doh_socket), doh_ssl_ctx);

		if (!SSL_set_tlsext_host_name(
			ssl_stream.native_handle(), std::string(doh_host).c_str()))
		{
			XLOG_DBG << "doh set sni name: " << doh_host << " failed";
		}

		co_await ssl_stream.async_handshake(
			net::ssl::stream_base::client, net_awaitable[ec]);
		if (ec)
		{
			XLOG_WARN << "doh tls handshake with " << doh_host << " failed";
			co_return false;
		}

		// 构造 HTTP POST 请求.
		http::request<http::string_body> doh_req{
			http::verb::post, doh_path, 11 };
		doh_req.set(http::field::host, doh_host);
		doh_req.set(http::field::content_type, "application/dns-message");
		doh_req.set(http::field::accept, "application/dns-message");
		doh_req.body() = dns_query;
		doh_req.prepare_payload();

		co_await http::async_write(ssl_stream, doh_req, net_awaitable[ec]);
		if (ec)
			co_return false;

		beast::flat_buffer buf;
		http::response<http::string_body> doh_res;
		co_await http::async_read(ssl_stream, buf, doh_res, net_awaitable[ec]);
		if (ec)
			co_return false;

		if (doh_res.result() != http::status::ok)
		{
			XLOG_WARN << "doh query raw response status: "
				<< doh_res.result_int();
			co_return false;
		}

		output = std::move(doh_res.body());
		co_return true;
	}

	// resolve_normal 按系统默认解析流程处理 DNS 查询并构造响应.
	net::awaitable<void> dns_server::resolve_normal(
		const std::string& dns_query, std::string& output)
	{
		std::string qname;
		uint16_t qtype = DNS_TYPE_A;
		if (!proxy_session::dns_parse_query(dns_query, qname, qtype))
		{
			// 无法解析的查询返回 FORMERR.
			output = proxy_session::dns_build_response(dns_query, 1, {});
			co_return;
		}

		// 禁用 IPv6 解析返回：AAAA 查询返回空应答（NODATA）.
		if (m_option.dns_no_ipv6_ && qtype == DNS_TYPE_AAAA)
		{
			output = proxy_session::dns_build_response(dns_query, 0, {});
			co_return;
		}

		switch (qtype)
		{
		case DNS_TYPE_A:
		case DNS_TYPE_AAAA:
		{
			// 在 backend 执行上下文执行同步解析.
			boost::system::error_code ec;
			auto ex = co_await backend_switch_to(
				m_scheduler_locking, m_backend_context, m_executor);

			tcp::resolver resolver{ ex };
			auto targets = co_await resolver.async_resolve(
				qname, "", net_awaitable[ec]);

			co_await backend_switch_from(m_scheduler_locking, m_executor);

			// 查询失败统一返回 NXDOMAIN.
			if (ec)
			{
				output = proxy_session::dns_build_response(dns_query, 3, {});
				co_return;
			}

			std::vector<proxy_session::dns_answer> answers;
			for (const auto& t : targets)
			{
				auto addr = t.endpoint().address();
				if (qtype == DNS_TYPE_A)
				{
					if (addr.is_v4())
					{
						auto bytes = addr.to_v4().to_bytes();
						std::string data(
							reinterpret_cast<const char*>(bytes.data()),
							bytes.size());
						answers.push_back({
							qname + ".", DNS_TYPE_A, dns_local_ttl,
							std::move(data) });
					}
				}
				else
				{
					if (addr.is_v6())
					{
						auto bytes = addr.to_v6().to_bytes();
						std::string data(
							reinterpret_cast<const char*>(bytes.data()),
							bytes.size());
						answers.push_back({
							qname + ".", DNS_TYPE_AAAA, dns_local_ttl,
							std::move(data) });
					}
				}
			}

			output = proxy_session::dns_build_response(dns_query, 0, answers);
			co_return;
		}
		default:
			// 其余类型（CNAME/MX/TXT/SOA 等）返回 NOERROR 且无应答.
			output = proxy_session::dns_build_response(dns_query, 0, {});
			co_return;
		}
	}

}

//////////////////////////////////////////////////////////////////////////
