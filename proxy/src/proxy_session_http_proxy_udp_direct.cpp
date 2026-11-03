//
// proxy_session_http_proxy_udp_direct.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/proxy_session.hpp"
#include "proxy/authority.hpp"
#include "proxy/http_error_body.hpp"
#include "proxy/proxy_websocket_client.hpp"

#include <array>
#include <charconv>

namespace proxy {

	net::awaitable<void> proxy_session::http_connect_udp_direct(
		const http::request<http::string_body>& req,
		const std::string& target_host,
		uint16_t target_port) noexcept
	{
		boost::system::error_code ec;

		// 以下为终端代理场景: 无 proxy_pass, 直接与目标 UDP 服务器通信.
		log_conn_debug()
			<< ", connect-udp: final proxy, creating UDP socket to "
			<< target_host << ":" << target_port;

		// 解析目标主机 (如果是域名需要 DNS 解析).
		auto protocol = udp::v4();
		net::ip::address target_addr;

		// 尝试将目标主机解析为 IP 地址, 若发生错误, 则说明是域名需要通过 DNS 解析.
		auto addr = net::ip::make_address(target_host, ec);
		if (ec)
		{
			co_await switch_to_backend_executor();

			udp::resolver resolver(m_backend_context);
			auto results = co_await resolver.async_resolve(
				target_host, std::to_string(target_port), net_awaitable[ec]);

			co_await switch_from_backend_executor();

			if (ec || results.empty())
			{
				log_conn_warning()
					<< ", connect-udp: DNS resolve failed for "
					<< target_host << ": " << ec.message();

				http::response<http::string_body> res{
					http::status::bad_gateway,
					req.version() };

				res.set(http::field::content_type, "text/html");
				res.set("Proxy-Status", "dns_error");
				res.body() = fake_502_dns_content;
				res.prepare_payload();

				co_await http::async_write(
					m_local_socket, res, net_awaitable[ec]);

				co_return;
			}

			// 解析成功, 从结果中提取地址, 只取第一个地址即可.
			target_addr = results.begin()->endpoint().address();
			protocol = target_addr.is_v4() ? udp::v4() : udp::v6();
		}
		else
		{
			target_addr = addr;
			protocol = target_addr.is_v4() ? udp::v4() : udp::v6();
		}

		// 创建并连接 UDP socket, 专为目标地址的 udp socket 服务.
		udp::socket udp_socket(m_executor);
		udp_socket.open(protocol, ec);
		if (ec)
		{
			log_conn_warning()
				<< ", connect-udp: open UDP socket: " << ec.message();
			co_return;
		}

		// 如果有绑定接口, 则绑定到指定接口.
		if (m_bind_interface)
		{
			udp::endpoint bind_endpoint(*m_bind_interface, 0);
			udp_socket.bind(bind_endpoint, ec);
			if (ec)
			{
				log_conn_warning()
					<< ", connect-udp: bind UDP socket: " << ec.message();
				co_return;
			}
		}

		// 设置 SO_MARK.
		if (m_option.so_mark_)
		{
			auto ret = apply_so_mark(
				udp_socket.native_handle(), m_option.so_mark_);
			if (ret.has_error())
			{
				log_conn_warning()
					<< ", connect-udp: set socket mark: "
					<< ret.error().message();
			}
		}

		// "连接" UDP socket 到目标地址.
		udp::endpoint target_endpoint(target_addr, target_port);
		udp_socket.connect(target_endpoint, ec);
		if (ec)
		{
			log_conn_warning()
				<< ", connect-udp: connect UDP socket to "
				<< target_host << ":" << target_port
				<< " error: " << ec.message();
			co_return;
		}

		log_conn_debug()
			<< ", connect-udp: UDP socket connected to "
			<< target_endpoint;

		// 发送 101 Switching Protocols 响应.
		{
			http::response<http::empty_body> res;
			res.version(11);
			res.result(http::status::switching_protocols);
			res.set(http::field::connection, "Upgrade");
			res.set(http::field::upgrade, "connect-udp");
			res.set("Capsule-Protocol", "?1");

			co_await http::async_write(
				m_local_socket, res, net_awaitable[ec]);
			if (ec)
			{
				log_conn_warning()
					<< ", connect-udp: send 101 response: "
					<< ec.message();
				co_return;
			}
		}

		log_conn_debug()
			<< ", connect-udp: 101 sent, starting capsule/UDP forwarding";

		stream_expires_never(m_local_socket);
		m_last_activity.store(0, std::memory_order_relaxed);
		m_transfer_timeout.store(
			m_option.udp_timeout_, std::memory_order_relaxed);
		co_await run_connected_udp_bridge(udp_socket);

		log_conn_debug()
			<< ", connect-udp: forwarding completed for "
			<< target_host << ":" << target_port;

		co_return;
	}

} // namespace proxy
