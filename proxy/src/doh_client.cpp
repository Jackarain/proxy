//
// doh_client.cpp
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/doh_client.hpp"

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <algorithm>
#include <utility>

namespace proxy {

	//////////////////////////////////////////////////////////////////////////
	// connect_proxy_pass

	net::awaitable<tcp::socket> connect_proxy_pass(
		net::any_io_executor executor,
		const proxy_server_option& opt,
		const urls::url& proxy_url,
		const std::function<net::awaitable<bool>(int)>& protect)
	{
		boost::system::error_code ec;

		std::string proxy_host(proxy_url.encoded_host());
		uint16_t proxy_port = proxy_url.port_number();
		if (proxy_port == 0)
			proxy_port = proxy_pass_default_port(proxy_url);

		tcp::resolver resolver(executor);
		auto targets = co_await resolver.async_resolve(
			proxy_host, std::to_string(proxy_port), net_awaitable[ec]);
		if (ec || targets.empty())
		{
			XLOG_WARN << "tun resolve proxy_pass: " << proxy_host
				<< ", error: " << ec.message();
			co_return tcp::socket(executor);
		}

		for (const auto& t : targets)
		{
			tcp::socket sock(executor);
			sock.open(t.endpoint().protocol(), ec);
			if (ec)
				continue;

			// 设置 SO_MARK（配合策略路由排除代理自身流量，防止环路）.
			apply_so_mark_if(sock, opt);

			// 先放行再 connect，防止 SYN 回环进 tun 形成环路.
			if (protect && !co_await protect(sock.native_handle()))
				continue;

			co_await sock.async_connect(t.endpoint(), net_awaitable[ec]);
			if (!ec)
				co_return sock;
		}

		XLOG_WARN << "tun connect proxy_pass: " << proxy_host
			<< ", error: " << ec.message();
		co_return tcp::socket(executor);
	}

	//////////////////////////////////////////////////////////////////////////
	// doh_connection

	doh_connection::doh_connection(net::any_io_executor executor,
		proxy_server_option opt,
		std::function<net::awaitable<bool>(int)> protect)
		: m_executor(std::move(executor))
		, m_option(std::move(opt))
		, m_protect(std::move(protect))
		, m_idle_timer(m_executor)
		, m_req_timer(m_executor)
	{
		// 解析 DoH 目标（未配置 dns_doh_ 时把 proxy_pass 当作 DoH 服务）.
		const auto& proxy_url = *m_option.proxy_pass_;
		m_doh_host = std::string(proxy_url.encoded_host());
		m_doh_port = proxy_url.port_number();
		if (m_doh_port == 0)
			m_doh_port = urls::default_port(proxy_url.scheme_id());
		if (m_doh_port == 0)
			m_doh_port = 443;
		m_doh_path = "/dns-query";

		if (!m_option.dns_doh_.empty())
		{
			if (auto r = urls::parse_uri(m_option.dns_doh_); r.has_value())
			{
				m_doh_host = std::string(r->encoded_host());
				if (!r->path().empty())
					m_doh_path = std::string(r->path());
				m_doh_port = r->port_number();
				if (m_doh_port == 0)
					m_doh_port = urls::default_port(r->scheme_id());
				if (m_doh_port == 0)
					m_doh_port = 443;
				auto scheme = boost::to_lower_copy(std::string(r->scheme()));
				m_doh_https = scheme.ends_with("s");
			}
		}

		// 与 proxy_pass 同服务为直连 DoH，否则经代理 CONNECT 隧道转发.
		auto proxy_host = boost::to_lower_copy(
			std::string(proxy_url.encoded_host()));
		auto doh_host = boost::to_lower_copy(m_doh_host);
		m_doh_via_proxy = !doh_host.empty() && doh_host != proxy_host;

		m_auth_user = std::string(proxy_url.user());
		m_auth_pass = std::string(proxy_url.password());
	}

	doh_connection::~doh_connection()
	{
		close();
	}

	void doh_connection::submit(std::string dns_query,
		std::function<void(const boost::system::error_code&, std::string)> handler)
	{
		if (m_closed || m_queue.size() >= k_max_pending)
		{
			if (handler)
				handler(make_error_code(
					boost::system::errc::connection_aborted), {});
			return;
		}

		m_queue.push_back({ std::move(dns_query), std::move(handler) });
		m_idle_timer.cancel();

		// 首次提交启动 pump 协程（空闲退出后由后续 submit 重启）.
		if (!m_pumping)
		{
			m_pumping = true;
			net::co_spawn(m_executor,
				[self = shared_from_this()]() -> net::awaitable<void>
				{
					co_await self->pump();
				}, net::detached);
		}
	}

	size_t doh_connection::pending() const noexcept
	{
		return m_queue.size() + (m_busy ? 1 : 0);
	}

	void doh_connection::close()
	{
		if (m_closed)
			return;
		m_closed = true;

		teardown();
		m_idle_timer.cancel();
		m_req_timer.cancel();

		// 唤醒所有排队请求（失败）.
		auto queue = std::move(m_queue);
		m_queue.clear();
		const auto ec = make_error_code(
			boost::system::errc::connection_aborted);
		for (auto& req : queue)
		{
			if (req.handler)
				req.handler(ec, {});
		}
	}

	void doh_connection::teardown()
	{
		if (m_stream)
		{
			boost::system::error_code ec;
			m_stream->close(ec);
			m_stream.reset();
		}
	}

	net::awaitable<void> doh_connection::pump()
	{
		for (;;)
		{
			if (m_closed)
			{
				m_pumping = false;
				co_return;
			}

			if (m_queue.empty())
			{
				// 空闲：等待新请求或超时关闭.
				m_idle_timer.expires_after(k_idle_timeout);
				boost::system::error_code ec;
				co_await m_idle_timer.async_wait(net_awaitable[ec]);
				if (ec != net::error::operation_aborted)
				{
					// 空闲超时，关闭连接释放资源.
					m_pumping = false;
					close();
					co_return;
				}
				// 被唤醒：新请求到达或连接被关闭.
				if (m_closed)
				{
					m_pumping = false;
					co_return;
				}
				continue;
			}

			// 取出队首请求串行处理.
			auto req = std::move(m_queue.front());
			m_queue.pop_front();
			m_busy = true;

			std::string response;
			bool ok = false;
			for (int attempt = 0; attempt < 2 && !m_closed; ++attempt)
			{
				if (!m_stream && !co_await connect())
					break;  // 建连失败，重试无意义.
				if (m_closed)
					break;
				ok = co_await exchange(req.dns_query, response);
				if (ok)
					break;
				// 连接损坏，关闭后重建连接重试一次.
				teardown();
			}
			m_busy = false;

			if (req.handler)
			{
				if (ok)
					req.handler({}, std::move(response));
				else
					req.handler(make_error_code(
						boost::system::errc::connection_aborted), {});
			}
		}
	}

	net::awaitable<bool> doh_connection::connect()
	{
		const auto& proxy_url = *m_option.proxy_pass_;

		auto sock = co_await connect_proxy_pass(
			m_executor, m_option, proxy_url, m_protect);
		if (!sock.is_open())
			co_return false;

		// 经代理 CONNECT 隧道转发，或与代理同服务直连.
		co_return m_doh_via_proxy ?
			co_await connect_via_proxy(std::move(sock)) :
			co_await connect_direct(std::move(sock));
	}

	// connect_via_proxy 经代理 CONNECT 隧道转发到 DoH 服务：
	// https 代理先与代理建立外层 TLS，再建立 CONNECT 隧道，
	// 隧道内按需与 DoH 服务建立内层 TLS.
	net::awaitable<bool> doh_connection::connect_via_proxy(tcp::socket sock)
	{
		boost::system::error_code ec;

		const auto& proxy_url = *m_option.proxy_pass_;
		const std::string proxy_host(proxy_url.encoded_host());
		const std::string authority =
			m_doh_host + ":" + std::to_string(m_doh_port);
		const std::string proxy_sni = m_option.proxy_ssl_name_.empty() ?
			proxy_host : m_option.proxy_ssl_name_;

		// https 代理先与代理建立外层 TLS.
		std::optional<ssl_tcp_stream> outer;
		std::optional<net::ssl::context> outer_ctx;
		if (proxy_use_ssl(proxy_url, m_option))
		{
			outer_ctx.emplace(net::ssl::context::sslv23_client);
			ec = configure_ssl_client_ctx(*outer_ctx,
				m_option.disable_check_cert_, proxy_sni,
				m_option.ssl_cacert_path_);
			if (ec)
				co_return false;
			outer.emplace(std::move(sock), *outer_ctx);
			SSL_set_tlsext_host_name(
				outer->native_handle(), proxy_sni.c_str());
			co_await outer->async_handshake(
				net::ssl::stream_base::client, net_awaitable[ec]);
			if (ec)
				co_return false;
		}

		// 建立 CONNECT 隧道.
		if (outer)
		{
			if (!co_await http_connect_tunnel(
				*outer, authority, proxy_url))
				co_return false;
		}
		else
		{
			if (!co_await http_connect_tunnel(
				sock, authority, proxy_url))
				co_return false;
		}

		// 取出隧道底层 TCP socket 供内层 TLS 复用.
		tcp::socket tunnel = outer ?
			std::move(outer->next_layer().lowest_layer()) :
			std::move(sock);

		if (!m_doh_https)
		{
			m_stream.emplace(init_proxy_stream(std::move(tunnel)));
			co_return true;
		}

		if (!co_await setup_tls(m_doh_host))
			co_return false;
		m_stream.emplace(init_proxy_stream(std::move(tunnel), *m_ssl_ctx));
		co_return co_await tls_handshake(m_doh_host);
	}

	// connect_direct 直连 DoH 服务：与 proxy_pass 同服务（或未配置
	// dns_doh_ 时把 proxy_pass 当作 DoH 服务器），https 时走 TLS.
	net::awaitable<bool> doh_connection::connect_direct(tcp::socket sock)
	{
		const auto& proxy_url = *m_option.proxy_pass_;
		const std::string proxy_host(proxy_url.encoded_host());

		if (!proxy_use_ssl(proxy_url, m_option))
		{
			m_stream.emplace(init_proxy_stream(std::move(sock)));
			co_return true;
		}

		const std::string sni = m_option.proxy_ssl_name_.empty() ?
			proxy_host : m_option.proxy_ssl_name_;

		if (!co_await setup_tls(sni))
			co_return false;
		m_stream.emplace(init_proxy_stream(std::move(sock), *m_ssl_ctx));
		co_return co_await tls_handshake(sni);
	}

	// setup_tls 惰性初始化 SSL context，并以 verify_host 配置证书校验与 SNI.
	net::awaitable<bool> doh_connection::setup_tls(
		const std::string& verify_host)
	{
		boost::system::error_code ec;

		if (!m_ssl_ctx)
		{
			m_ssl_ctx.emplace(net::ssl::context::sslv23_client);
			m_ssl_configured = false;
		}
		if (!m_ssl_configured)
		{
			ec = configure_ssl_client_ctx(*m_ssl_ctx,
				m_option.disable_check_cert_, verify_host,
				m_option.ssl_cacert_path_);
			if (ec)
				co_return false;
			m_ssl_configured = true;
		}
		co_return true;
	}

	// tls_handshake 对 m_stream 中的 SSL 流设置 SNI 并完成客户端握手，
	// 失败时销毁流后返回 false.
	net::awaitable<bool> doh_connection::tls_handshake(
		const std::string& host)
	{
		boost::system::error_code ec;
		auto& ssl = boost::variant2::get<ssl_tcp_stream>(*m_stream);
		SSL_set_tlsext_host_name(ssl.native_handle(), host.c_str());
		co_await ssl.async_handshake(
			net::ssl::stream_base::client, net_awaitable[ec]);
		if (ec)
		{
			m_stream.reset();
			co_return false;
		}
		co_return true;
	}

	net::awaitable<bool> doh_connection::exchange(
		const std::string& dns_query, std::string& output)
	{
		if (!m_stream)
			co_return false;

		// 单次请求超时：超时后关闭连接使挂起的读写立即失败.
		// 用 weak_ptr 防止连接析构后 handler 访问已销毁对象.
		m_req_timer.expires_after(k_request_timeout);
		std::weak_ptr<doh_connection> weak = shared_from_this();
		m_req_timer.async_wait(
			[weak](const boost::system::error_code& ec)
			{
				if (ec)
					return;
				if (auto self = weak.lock();
					self && !self->m_closed)
					self->teardown();
			});

		// 经代理隧道转发时认证头仅用于代理 CONNECT（由 http_connect_tunnel
		// 发送），不向 DoH 服务器附带；直连同服务时透传 proxy 的 Basic 认证.
		bool ok = co_await doh_post(*m_stream, m_doh_host, m_doh_path,
			dns_query, output,
			m_doh_via_proxy ? std::string{} : m_auth_user,
			m_doh_via_proxy ? std::string{} : m_auth_pass);

		m_req_timer.cancel();
		co_return ok;
	}

	//////////////////////////////////////////////////////////////////////////
	// doh_client

	doh_client::doh_client(net::any_io_executor executor,
		proxy_server_option opt,
		std::function<net::awaitable<bool>(int)> protect)
		: m_executor(std::move(executor))
		, m_option(std::move(opt))
		, m_protect(std::move(protect))
	{}

	net::awaitable<std::string> doh_client::query(
		const std::string& dns_query)
	{
		struct state
		{
			boost::system::error_code ec;
			std::string result;
			bool ready { false };
			net::steady_timer timer;
			explicit state(net::any_io_executor ex)
				: timer(ex)
			{
				// 挂起到 handler 回调：期满前 cancel 唤醒即可退出.
				timer.expires_at(
					std::chrono::steady_clock::time_point::max());
			}
		};

		auto st = std::make_shared<state>(m_executor);

		pick()->submit(dns_query,
			[st](const boost::system::error_code& ec, std::string result)
			{
				st->ec = ec;
				st->result = std::move(result);
				st->ready = true;
				st->timer.cancel();
			});

		while (!st->ready)
		{
			boost::system::error_code ec;
			co_await st->timer.async_wait(net_awaitable[ec]);
		}

		co_return std::move(st->result);
	}

	void doh_client::close()
	{
		for (auto& conn : m_conns)
			conn->close();
		m_conns.clear();
	}

	std::shared_ptr<doh_connection> doh_client::pick()
	{
		// 移除已关闭的连接.
		m_conns.erase(std::remove_if(m_conns.begin(), m_conns.end(),
			[](const auto& c) { return c->closed(); }), m_conns.end());

		// 优先复用空闲连接.
		for (auto& c : m_conns)
			if (c->pending() == 0)
				return c;

		// 未达上限时新建连接.
		if (m_conns.size() < k_max_connections)
		{
			auto conn = std::make_shared<doh_connection>(
				m_executor, m_option, m_protect);
			m_conns.push_back(conn);
			return conn;
		}

		// 已满：选择排队最少的连接.
		auto it = std::min_element(m_conns.begin(), m_conns.end(),
			[](const auto& a, const auto& b)
			{
				return a->pending() < b->pending();
			});
		return *it;
	}

} // namespace proxy
