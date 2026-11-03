//
// proxy_session_http_proxy_udp_proxy_websocket.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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

	net::awaitable<void> proxy_session::http_udp_proxy_pass_websocket(
		const http::request<http::string_body>& req,
		const std::string& target_host,
		uint16_t target_port)
	{
		boost::system::error_code ec;

		if (!co_await connect_websocket_udp_proxy_pass(
			target_host, target_port))
		{
			http::response<http::string_body> res{
				http::status::bad_gateway, req.version() };
			res.set(http::field::content_type, "text/plain");
			res.body() = "Failed to establish upstream WebSocket UDP tunnel.\n";
			res.prepare_payload();
			co_await http::async_write(
				m_local_socket, res, net_awaitable[ec]);
			co_return;
		}

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
			stream_cancel(m_remote_socket);
			co_return;
		}

		stream_expires_never(m_local_socket);
		co_await http_capsules_to_websocket_proxy();
		co_return;
	}

} // namespace proxy
