//
// proxy_session_resolve.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/proxy_session.hpp"
#include "proxy/http_error_body.hpp"
#include "proxy/proxy_websocket_client.hpp"

namespace proxy {


	net::awaitable<boost::system::error_code> proxy_session::start_connect_host(
		std::string target_host,
		uint16_t target_port,
		bool resolved,
		int command) noexcept
	{
		// 记录本次会话的代理目标（供 launcher 连接列表上报）.
		set_target(target_host + ":" + std::to_string(target_port));

		tcp::socket& remote_socket = net_tcp_socket(m_remote_socket);
		tcp::resolver::results_type targets;
		boost::system::error_code ec;

		if (m_proxy_pass)
		{
			// 查询网关代理中继服务器域名信息.
			targets = co_await resolve_proxy_pass_targets();

			// 发起连接到网关代理中继服务器.
			ec = co_await connect_proxy_pass(remote_socket, targets);
			if (!ec)
			{
				// 与网关代理中继服务器握手.
				ec = co_await proxy_pass_handshake(remote_socket,
					target_host, target_port,
					*m_proxy_pass, command);
			}

			co_return ec;
		}
		else
		{
			switch (static_cast<int>(resolved))
			{
			case 1: // true
			{
				tcp::endpoint dst_endpoint;

				auto addr = net::ip::make_address(target_host, ec);
				if (!ec)
				{
					dst_endpoint.address(addr);
					dst_endpoint.port(target_port);

					targets = net::ip::basic_resolver_results<tcp>::create(dst_endpoint, "", "");
					break;
				}
			}
			[[fallthrough]];
			case 0:  // false
			default:
				targets = co_await resolve_targets(target_host, target_port);
				break;
			}
		}

		// 直接连接到目标机器.
		ec = co_await async_connect_targets(remote_socket, targets);
		if (ec)
			co_return ec;

		// 重新初始化 m_remote_socket 为 variant_stream_type 类型.
		m_remote_socket = init_proxy_stream(std::move(remote_socket));

		co_return ec;
	}

	//////////////////////////////////////////////////////////////////////////
	// DNS 解析与缓存

	tcp::resolver::results_type
	proxy_session::get_resolver_from_cache(const std::string& hostname, uint16_t port) noexcept
	{
		auto dns = m_dns_cache.get(hostname);
		if (dns)
		{
			[[maybe_unused]] auto& [result, tp] = *dns;

			std::vector<tcp::endpoint> new_endpoints;
			new_endpoints.reserve(result.size());

			std::transform(result.begin(), result.end(),
				std::back_inserter(new_endpoints),
				[port](const tcp::endpoint& ep)
				{
					return tcp::endpoint(ep.address(), port);
				});

			return tcp::resolver::results_type::create(
				new_endpoints.begin(), new_endpoints.end(),
				hostname, std::to_string(port));
		}

		return {};
	}

	net::awaitable<net::any_io_executor> proxy_session::switch_to_backend_executor()
	{
		co_return co_await backend_switch_to(
			m_scheduler_locking, m_backend_context, m_executor);
	}

	net::awaitable<void> proxy_session::switch_from_backend_executor()
	{
		co_await backend_switch_from(m_scheduler_locking, m_executor);
	}

	net::awaitable<tcp::resolver::results_type>
	proxy_session::resolve_targets(const std::string& hostname, uint16_t port_number) noexcept
	{
		net::ip::basic_resolver_results<tcp> targets;

		targets = get_resolver_from_cache(hostname, port_number);
		if (!targets.empty())
			co_return targets;

		boost::system::error_code ec;

		auto executor = co_await switch_to_backend_executor();

		tcp::resolver resolver{ executor };

		targets = co_await resolver.async_resolve(
			hostname,
			std::to_string(port_number),
			net_awaitable[ec]);

		co_await switch_from_backend_executor();

		if (ec)
		{
			log_conn_warning()
				<< ", resolve: "
				<< hostname
				<< ", error: "
				<< ec.message();

			co_return targets;
		}

		// 更新 dns 缓存.
		dns_result result = std::make_tuple(targets, std::chrono::steady_clock::now());
		m_dns_cache.put(hostname, std::move(result));

		co_return targets;
	}

	net::awaitable<tcp::resolver::results_type>
	proxy_session::resolve_proxy_pass_targets() noexcept
	{
		tcp::resolver::results_type targets;

		if (!m_proxy_pass)
			co_return targets;

		auto proxy_host = std::string(m_proxy_pass->host_address());
		uint16_t proxy_port_number = 0;

		if (m_proxy_pass->port_number() == 0)
			proxy_port_number = proxy_pass_default_port(*m_proxy_pass);
		else
			proxy_port_number = m_proxy_pass->port_number();

		// 如果是 IP 地址, 则直接构造而不需要 DNS 查询.
		if (!is_hostname(proxy_host))
		{
			net::ip::tcp::endpoint endp(
				net::ip::make_address(proxy_host), proxy_port_number);

			targets = tcp::resolver::results_type::create(
				endp, proxy_host, m_proxy_pass->scheme());
		}
		else
		{
			// 进行 DNS 查询.
			targets = co_await resolve_targets(proxy_host, proxy_port_number);
		}

		co_return targets;
	}

	//////////////////////////////////////////////////////////////////////////

	net::awaitable<boost::system::error_code>
	proxy_session::async_connect_targets(tcp::socket& socket, tcp::resolver::results_type& targets) noexcept
	{
		boost::system::error_code ec;

		if (targets.empty())
		{
			ec = net::error::host_not_found;
			co_return ec;
		}

		if (m_option.happyeyeballs_)
		{
			co_await asio_util::async_connect(
				socket,
				targets,
				[this](const auto&, auto& stream, auto& endp)
				{
					boost::system::error_code ec;

					if (m_bind_interface)
					{
						tcp::endpoint bind_endpoint(*m_bind_interface, 0);

						stream.open(bind_endpoint.protocol(), ec);
						if (ec)
							return false;

						stream.bind(bind_endpoint, ec);
						if (ec)
							return false;
					}
					else
					{
						stream.open(endp.endpoint().protocol(), ec);
						if (ec)
							return false;
					}

					// 路由决策发生在 connect 时，须在连接前设置 SO_MARK.
					if (m_option.so_mark_)
					{
						auto ret = apply_so_mark(stream.native_handle(), m_option.so_mark_);
						if (ret.has_error())
						{
							log_conn_warning()
								<< ", set socket mark error: "
								<< ret.error().message();
						}
					}

					return true;
				},
				net_awaitable[ec]);

			{
				auto ret = set_tcp_keepalive(socket.native_handle());
				if (ret.has_error())
				{
					log_conn_warning()
						<< ", tcp keepalive error: "
						<< ret.error().message();
				}
			}

			// 禁用 Nagle 算法, 使小数据包(如 HTTP 头、SOCKS 握手)立即发送.
			{
				auto ret = set_tcp_nodelay(socket.native_handle(), true);
				if (ret.has_error())
				{
					log_conn_warning()
						<< ", tcp nodelay error: "
						<< ret.error().message();
				}
			}

#if defined(__linux__)
			// 启用 TCP_QUICKACK, 减少 ACK 延迟, 提升交互式代理场景的响应速率.
			{
				auto ret = set_tcp_quickack(socket.native_handle(), true);
				if (ret.has_error())
				{
					log_conn_warning()
						<< ", tcp quickack error: "
						<< ret.error().message();
				}
			}
#endif

			// 增大 TCP 缓冲区以提升吞吐量 (256KB 接收, 256KB 发送).
			{
				auto ret = set_tcp_buffer_sizes(
					socket.native_handle(), 256 * 1024, 256 * 1024);
				if (ret.has_error())
				{
					log_conn_warning()
						<< ", tcp buffer sizes error: "
						<< ret.error().message();
				}
			}

			co_return ec;
		}

		for (const auto& endpoint : targets)
		{
			ec = boost::asio::error::host_not_found;

			if (m_option.connect_v4_only_)
			{
				if (endpoint.endpoint().address().is_v6())
				{
					XLOG_DBG << "connect_v4_only: skip IPv6 endpoint "
						<< endpoint.endpoint();
					continue;
				}
			}
			else if (m_option.connect_v6_only_)
			{
				if (endpoint.endpoint().address().is_v4())
				{
					XLOG_DBG << "connect_v6_only: skip IPv4 endpoint "
						<< endpoint.endpoint();
					continue;
				}
			}

			boost::system::error_code ignore_ec;
			socket.close(ignore_ec);

			if (m_bind_interface)
			{
				tcp::endpoint bind_endpoint(
					*m_bind_interface,
					0);

				socket.open(
					bind_endpoint.protocol(),
					ec);
				if (ec)
					break;

				socket.bind(
					bind_endpoint,
					ec);
				if (ec)
					break;
			}
			else
			{
				socket.open(endpoint.endpoint().protocol(), ec);
				if (ec)
					break;
			}

			// 路由决策发生在 connect 时，须在连接前设置 SO_MARK.
			if (m_option.so_mark_)
			{
				auto ret = apply_so_mark(socket.native_handle(), m_option.so_mark_);
				if (ret.has_error())
				{
					log_conn_warning()
						<< ", set socket mark error: "
						<< ret.error().message();
				}
			}

			co_await socket.async_connect(
				endpoint,
				net_awaitable[ec]);
			if (!ec)
			{
				{
					auto ret = set_tcp_keepalive(socket.native_handle());
					if (ret.has_error())
					{
						log_conn_warning()
							<< ", tcp keepalive error: "
							<< ret.error().message();
					}
				}

				// 禁用 Nagle 算法, 使小数据包立即发送.
				{
					auto ret = set_tcp_nodelay(socket.native_handle(), true);
					if (ret.has_error())
					{
						log_conn_warning()
							<< ", tcp nodelay error: "
							<< ret.error().message();
					}
				}

#if defined(__linux__)
				// 启用 TCP_QUICKACK, 减少 ACK 延迟.
				{
					auto ret = set_tcp_quickack(socket.native_handle(), true);
					if (ret.has_error())
					{
						log_conn_warning()
							<< ", tcp quickack error: "
							<< ret.error().message();
					}
				}
#endif

				// 增大 TCP 缓冲区以提升吞吐量.
				{
					auto ret = set_tcp_buffer_sizes(
						socket.native_handle(), 256 * 1024, 256 * 1024);
					if (ret.has_error())
					{
						log_conn_warning()
							<< ", tcp buffer sizes error: "
							<< ret.error().message();
					}
				}

				break;
			}
		}

		co_return ec;
	}

} // namespace proxy
