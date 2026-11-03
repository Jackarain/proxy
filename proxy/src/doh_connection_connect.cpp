//
// doh_connection_connect.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/doh_client.hpp"
#include "proxy/authority.hpp"
#include "proxy/proxy_websocket_client.hpp"

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <utility>

namespace proxy {

	net::awaitable<bool> doh_connection::connect()
	{
		// 建连整体超时: 黑洞网络下（对端无响应）TCP/TLS/隧道任一步都
		// 可能无限挂起, 超时后 abort() 关闭 m_stream 使在途建连操作
		// 立即失败, 避免 pump 与排队请求被永久悬挂.
		m_connect_aborted = false;
		const auto seq = ++m_conn_seq;
		m_conn_timer.expires_after(k_connect_timeout);
		std::weak_ptr<doh_connection> weak = shared_from_this();
		m_conn_timer.async_wait(
			[weak, seq](const boost::system::error_code& ec)
			{
				if (ec)
					return;
				if (auto self = weak.lock();
					self && !self->m_closed && seq == self->m_conn_seq)
				{
					self->m_connect_aborted = true;
					self->abort();
				}
			});

		bool ok = co_await connect_impl();

		// 使已排队（cancel 无法撤回）的超时回调过期, 避免其在后续
		// 请求阶段误中止新连接.
		++m_conn_seq;
		m_conn_timer.cancel();
		co_return ok;
	}

	net::awaitable<bool> doh_connection::connect_impl()
	{
		// 优先复用 proxy_pass 预选连接池中已建立（TCP/TLS 已完成）的连接.
		if (m_proxy_pool)
		{
			// 仅在确有现成空闲连接时复用: 空闲连接可立即取走, 不会等待
			// 池维护协程新建连接（该等待不受本连接超时控制, 可能悬挂）.
			if (m_proxy_pool->idle_count() > 0)
			{
				auto conn = co_await m_proxy_pool->acquire();
				// 超时回调可能在 acquire 等待期间触发: 此时 m_stream 尚为
				// 空, abort() 无法中止任何在途操作, 故取到连接后须再检查
				// 中止标记, 避免超时后仍继续建立隧道/握手.
				if (m_closed || m_connect_aborted)
				{
					if (conn && conn->is_open())
					{
						boost::system::error_code ec;
						conn->close(ec);
					}
					co_return false;
				}
				if (conn && conn->is_open())
				{
					if (co_await connect_pooled(std::move(*conn)))
						co_return true;

					// 池中连接不可用（代理端已断开等），回退新建连接.
					XLOG_WARN << "tun doh pool connection failed, "
						"fallback connect";
				}
			}
		}

		// 新建到代理的 TCP 连接（流挂在 m_stream 上, 可被 abort 中止）.
		if (!co_await connect_tcp_proxy())
			co_return false;

		// 经代理 CONNECT 隧道转发，或与代理同服务直连.
		co_return m_doh_via_proxy ?
			co_await connect_via_proxy() :
			co_await connect_direct();
	}

	// connect_tcp_proxy 新建到代理的 TCP 连接.
	net::awaitable<bool> doh_connection::connect_tcp_proxy()
	{
		boost::system::error_code ec;
		const auto& proxy_url = *m_option.proxy_pass_;
		std::string proxy_host(proxy_url.host_address());
		uint16_t proxy_port = proxy_url.port_number();
		if (proxy_port == 0)
			proxy_port = proxy_pass_default_port(proxy_url);

		tcp::resolver resolver(m_executor);
		auto targets = co_await resolver.async_resolve(
			proxy_host, std::to_string(proxy_port), net_awaitable[ec]);
		if (ec || targets.empty())
		{
			XLOG_WARN << "tun resolve proxy_pass: " << proxy_host
				<< ", error: " << ec.message();
			co_return false;
		}

		for (const auto& t : targets)
		{
			// 建连被中止（超时）或连接已关闭: 不再尝试剩余地址.
			if (m_closed || m_connect_aborted)
				co_return false;

			// 连接中的流挂在 m_stream 上: abort()（建连超时/连接关闭）
			// 可随时关闭底层 socket 中止这次尝试, 不会残留悬挂的连接.
			m_stream.emplace(init_proxy_stream(m_executor));
			auto& sock = net_tcp_socket(*m_stream);
			sock.open(t.endpoint().protocol(), ec);
			if (ec)
			{
				m_stream.reset();
				continue;
			}

			// 设置 SO_MARK（配合策略路由排除代理自身流量，防止环路）.
			apply_so_mark_if(sock, m_option);

			// 先放行再 connect，防止 SYN 回环进 tun 形成环路.
			if (m_protect && !co_await m_protect(sock.native_handle()))
			{
				m_stream.reset();
				continue;
			}

			co_await sock.async_connect(t.endpoint(), net_awaitable[ec]);
			if (!ec)
				co_return true;
			m_stream.reset();
		}

		XLOG_WARN << "tun connect proxy_pass: " << proxy_host
			<< ", error: " << ec.message();
		co_return false;
	}

	// connect_pooled 基于连接池获取的已有连接建立 DoH 连接：
	// 池连接的外层 TLS（proxy 为 https 时）已握手完成，直接复用；
	// 经代理 CONNECT 隧道时在其上建隧道，隧道内按需建内层 DoH TLS.
	net::awaitable<bool> doh_connection::connect_pooled(
		variant_stream_type stream)
	{
		const auto& proxy_url = *m_option.proxy_pass_;

		// 池连接（TCP/TLS 已完成）先挂到 m_stream: 隧道建立过程也受
		// abort()（建连超时/关闭）控制.
		m_stream = std::move(stream);

		if (!m_doh_via_proxy)
			co_return true;  // 同服务: 池连接即为 DoH 连接.

		// 经代理 CONNECT 隧道转发到 DoH 服务.
		const std::string& authority = m_doh_authority;
		if (proxy_scheme_is_websocket(proxy_url.scheme()))
		{
			co_return co_await connect_websocket_tunnel(
				authority, proxy_url);
		}
		if (!co_await http_connect_tunnel(
			*m_stream, authority, proxy_url))
		{
			m_stream.reset();
			co_return false;
		}

		// 取出隧道底层 TCP socket 供内层 TLS 复用.
		tcp::socket tunnel = std::move(net_tcp_socket(*m_stream));
		m_stream.reset();

		if (!m_doh_https)
		{
			m_stream.emplace(init_proxy_stream(std::move(tunnel)));
			co_return true;
		}
		if (!co_await setup_tls(m_doh_host))
		{
			m_stream.reset();
			co_return false;
		}
		m_stream.emplace(init_proxy_stream(std::move(tunnel), *m_ssl_ctx));
		co_return co_await tls_handshake(m_doh_host);
	}

	// connect_via_proxy 经代理 CONNECT 隧道转发到 DoH 服务：
	// https 代理先与代理建立外层 TLS，再建立 CONNECT 隧道，
	// 隧道内按需与 DoH 服务建立内层 TLS.
	net::awaitable<bool> doh_connection::connect_via_proxy()
	{
		const auto& proxy_url = *m_option.proxy_pass_;
		const std::string proxy_host(proxy_url.host_address());
		const std::string& authority = m_doh_authority;
		const std::string proxy_sni = m_option.proxy_ssl_name_.empty() ?
			proxy_host : m_option.proxy_ssl_name_;

		// https 代理先与代理建立外层 TLS（流由 m_stream 持有, 可被中止）.
		if (proxy_use_ssl(proxy_url, m_option))
		{
			if (!m_proxy_ssl_ctx)
			{
				m_proxy_ssl_ctx.emplace(net::ssl::context::sslv23_client);
				boost::system::error_code ec;
				ec = configure_ssl_client_ctx(*m_proxy_ssl_ctx,
					m_option.disable_check_cert_, proxy_sni,
					m_option.ssl_cacert_path_);
				if (ec)
				{
					m_proxy_ssl_ctx.reset();
					m_stream.reset();
					co_return false;
				}
			}

			// 把明文 TCP 流升级为外层 TLS 流（当前无在途操作, 安全重建）.
			tcp::socket raw = std::move(net_tcp_socket(*m_stream));
			m_stream.reset();
			m_stream.emplace(init_proxy_stream(std::move(raw), *m_proxy_ssl_ctx));
			if (!co_await tls_handshake(proxy_sni))
				co_return false;
		}

		// 建立 CONNECT 隧道.
		if (proxy_scheme_is_websocket(proxy_url.scheme()))
		{
			co_return co_await connect_websocket_tunnel(
				authority, proxy_url);
		}

		if (!co_await http_connect_tunnel(
			*m_stream, authority, proxy_url))
		{
			m_stream.reset();
			co_return false;
		}

		// 取出隧道底层 TCP socket 供内层 TLS 复用.
		tcp::socket tunnel = std::move(net_tcp_socket(*m_stream));
		m_stream.reset();

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

	// connect_websocket_tunnel 在已完成的 TCP/TLS 外层连接上执行
	// WebSocket CONNECT 握手，并按需在隧道内与 DoH 服务建立 TLS.
	net::awaitable<bool> doh_connection::connect_websocket_tunnel(
		const std::string& authority, const urls::url& proxy_url)
	{
		auto options = make_websocket_handshake_options(
			proxy_url, authority);
		auto ec = co_await async_websocket_proxy_handshake(
			*m_stream, std::move(options));
		if (ec)
		{
			XLOG_WARN << "tun doh websocket tunnel: " << ec.message();
			m_stream.reset();
			co_return false;
		}

		if (!m_doh_https)
			co_return true;

		if (!co_await setup_tls(m_doh_host))
		{
			m_stream.reset();
			co_return false;
		}
		if (!websocket_stream_to_tls(*m_stream, *m_ssl_ctx))
		{
			m_stream.reset();
			co_return false;
		}
		co_return co_await tls_handshake(m_doh_host);
	}

	// connect_direct 直连 DoH 服务：与 proxy_pass 同服务（或未配置
	// dns_doh_ 时把 proxy_pass 当作 DoH 服务器），https 时走 TLS.
	net::awaitable<bool> doh_connection::connect_direct()
	{
		const auto& proxy_url = *m_option.proxy_pass_;

		if (!proxy_use_ssl(proxy_url, m_option))
			co_return true;  // 明文直连: m_stream 已是连接好的明文流.

		const std::string proxy_host(proxy_url.host_address());
		const std::string sni = m_option.proxy_ssl_name_.empty() ?
			proxy_host : m_option.proxy_ssl_name_;

		if (!co_await setup_tls(sni))
		{
			m_stream.reset();
			co_return false;
		}

		// 把明文 TCP 流升级为 TLS 流（当前无在途操作, 安全重建）.
		tcp::socket raw = std::move(net_tcp_socket(*m_stream));
		m_stream.reset();
		m_stream.emplace(init_proxy_stream(std::move(raw), *m_ssl_ctx));
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
		ec = co_await boost::variant2::visit(
			[&host](auto& ssl) -> net::awaitable<boost::system::error_code>
			{
				using StreamType = std::decay_t<decltype(ssl)>;
				if constexpr (std::same_as<StreamType, ssl_tcp_stream> ||
					std::same_as<StreamType, ssl_websocket_plain_stream> ||
					std::same_as<StreamType, ssl_websocket_tls_stream>)
				{
					boost::system::error_code ec;
					SSL_set_tlsext_host_name(
						ssl.native_handle(), host.c_str());
					co_await ssl.async_handshake(
						net::ssl::stream_base::client,
						net_awaitable[ec]);
					co_return ec;
				}
				else
				{
					co_return make_error_code(
						boost::system::errc::operation_not_supported);
				}
			}, *m_stream);
		if (ec)
		{
			m_stream.reset();
			co_return false;
		}
		co_return true;
	}

} // namespace proxy
