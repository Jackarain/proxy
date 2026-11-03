//
// proxy_session_http_proxy_websocket_udp_connect.cpp
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

	net::awaitable<void> proxy_session::http_proxy_websocket_connect_udp(
		const string_request& req,
		const std::string& target_host,
		uint16_t target_port) noexcept
	{
		auto send_error = [this, &req](http::status status,
			std::string_view message) -> net::awaitable<void>
		{
			http::response<http::string_body> res{ status, req.version() };
			res.set(http::field::server, version_string);
			res.set(http::field::date, server_date_string());
			res.set(http::field::content_type, "text/plain");
			res.keep_alive(false);
			res.body() = message;
			res.prepare_payload();

			boost::system::error_code ignore_ec;
			co_await http::async_write(
				m_local_socket, res, net_awaitable[ignore_ec]);
		};

		auto scheme = m_proxy_pass ?
			boost::to_lower_copy(std::string(m_proxy_pass->scheme())) :
			std::string();
		const bool ws_upstream = m_proxy_pass &&
			proxy_scheme_is_websocket(scheme);
		const bool downstream_tls =
			boost::variant2::holds_alternative<ssl_tcp_stream>(m_local_socket);

		if (m_option.disable_udp_)
		{
			co_await send_error(http::status::forbidden,
				"UDP proxy is disabled.\n");
			co_return;
		}

		if (m_proxy_pass && !ws_upstream)
		{
			co_await send_error(http::status::not_implemented,
				"WebSocket UDP requires a ws/wss proxy_pass.\n");
			co_return;
		}

		if (ws_upstream)
		{
			if (!co_await connect_websocket_udp_proxy_pass(
				target_host, target_port))
			{
				co_await send_error(http::status::bad_gateway,
					"Failed to establish upstream WebSocket UDP tunnel.\n");
				co_return;
			}

			auto ec = co_await accept_websocket_udp(req);
			if (ec)
			{
				log_conn_warning()
					<< ", websocket udp: downstream handshake: "
					<< ec.message();
				stream_cancel(m_remote_socket);
				co_return;
			}

			stream_expires_never(m_local_socket);
			set_proto(std::string(ws_tunnel::websocket_proto_name(
				downstream_tls, true)));
			co_await run_websocket_udp_bridge();
			co_return;
		}

		auto udp_socket = co_await open_udp_target(target_host, target_port);
		if (!udp_socket)
		{
			co_await send_error(http::status::bad_gateway,
				"Failed to open UDP target.\n");
			co_return;
		}

		auto ec = co_await accept_websocket_udp(req);
		if (ec)
		{
			log_conn_warning()
				<< ", websocket udp: downstream handshake: "
				<< ec.message();
			udp_socket->close(ec);
			co_return;
		}

		stream_expires_never(m_local_socket);
		set_proto(std::string(ws_tunnel::websocket_proto_name(
			downstream_tls, true)));
		co_await run_websocket_udp_target(*udp_socket);
	}

} // namespace proxy
