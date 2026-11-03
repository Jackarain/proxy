//
// proxy_session_http_proxy_websocket.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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
	net::awaitable<bool> proxy_session::http_proxy_websocket_connect(
		const string_request& req) noexcept
	{
		boost::system::error_code ec;

		auto send_error = [this, &req](http::status status,
			std::string_view authenticate = {}) -> net::awaitable<void>
		{
			http::response<http::string_body> res{ status, req.version() };
			res.set(http::field::server, version_string);
			res.set(http::field::date, server_date_string());
			res.keep_alive(false);
			if (!authenticate.empty())
				res.set(http::field::proxy_authenticate, authenticate);
			res.prepare_payload();

			boost::system::error_code ignore_ec;
			co_await http::async_write(
				m_local_socket, res, net_awaitable[ignore_ec]);
		};

		auto pa = std::string(req[http::field::proxy_authorization]);
		auto auth = co_await http_authorization(pa);
		if (auth != PROXY_AUTH_SUCCESS)
		{
			log_conn_warning()
				<< ", websocket proxy auth error: "
				<< pauth_error_message(auth);
			co_await send_error(
				http::status::proxy_authentication_required,
				"Basic realm=\"proxy\"");
			co_return true;
		}

		if (req.method() != http::verb::get || req.version() != 11 ||
			!beast::websocket::is_upgrade(req) ||
			req.find(http::field::host) == req.end())
		{
			co_await send_error(http::status::bad_request);
			co_return true;
		}

		auto tcp_count = req.count(ws_tunnel::proxy_connect_header) +
			req.count(ws_tunnel::proxy_connect_header_legacy);
		auto udp_count = req.count(ws_tunnel::proxy_connect_udp_header) +
			req.count(ws_tunnel::proxy_connect_udp_header_legacy);
		if (tcp_count + udp_count != 1)
		{
			co_await send_error(http::status::bad_request);
			co_return true;
		}

		const bool datagram = udp_count == 1;
		auto protocol = req[http::field::sec_websocket_protocol];
		auto expected_protocol = datagram ?
			ws_tunnel::proxy_connect_udp_subprotocol :
			ws_tunnel::proxy_connect_subprotocol;
		if (!ws_tunnel::header_has_token(protocol, expected_protocol))
		{
			co_await send_error(http::status::bad_request);
			co_return true;
		}

		auto target_it = datagram ?
			req.find(ws_tunnel::proxy_connect_udp_header) :
			req.find(ws_tunnel::proxy_connect_header);
		if (target_it == req.end())
		{
			target_it = req.find(datagram ?
				ws_tunnel::proxy_connect_udp_header_legacy :
				ws_tunnel::proxy_connect_header_legacy);
		}

		std::string target_host;
		uint16_t target_port = 0;
		if (!ws_tunnel::parse_authority(
			target_it->value(), target_host, target_port))
		{
			co_await send_error(http::status::bad_request);
			co_return true;
		}

		if (!boost::variant2::holds_alternative<proxy_tcp_socket>(m_local_socket) &&
			!boost::variant2::holds_alternative<ssl_tcp_stream>(m_local_socket))
		{
			co_await send_error(http::status::not_implemented);
			co_return true;
		}

		log_conn_debug()
			<< ", websocket proxy target: "
			<< target_host << ":" << target_port;

		const bool downstream_tls =
			boost::variant2::holds_alternative<ssl_tcp_stream>(m_local_socket);
		set_target(target_host + ":" + std::to_string(target_port));
		if (datagram)
		{
			co_await http_proxy_websocket_connect_udp(
				req, target_host, target_port);
			co_return true;
		}

		ec = co_await start_connect_host(target_host, target_port, false);
		if (ec)
		{
			log_conn_warning()
				<< ", websocket proxy connect target "
				<< target_host << ":" << target_port
				<< " error: " << ec.message();
			co_await send_error(http::status::bad_gateway);
			co_return true;
		}

		if (boost::variant2::holds_alternative<proxy_tcp_socket>(m_local_socket))
		{
			websocket_plain_stream websocket(
				std::move(boost::variant2::get<proxy_tcp_socket>(m_local_socket)));
			m_local_socket = init_proxy_stream(std::move(websocket));
			auto buffered = m_local_buffer.size();
			ec = co_await boost::variant2::get<websocket_plain_stream>(
				m_local_socket).async_accept(req, m_local_buffer.data());
			if (!ec)
				m_local_buffer.consume(buffered);
		}
		else
		{
			websocket_tls_stream websocket(
				std::move(boost::variant2::get<ssl_tcp_stream>(m_local_socket)));
			m_local_socket = init_proxy_stream(std::move(websocket));
			auto buffered = m_local_buffer.size();
			ec = co_await boost::variant2::get<websocket_tls_stream>(
				m_local_socket).async_accept(req, m_local_buffer.data());
			if (!ec)
				m_local_buffer.consume(buffered);
		}

		if (ec)
		{
			log_conn_warning()
				<< ", websocket proxy handshake error: " << ec.message();
			co_return true;
		}

		set_proto(std::string(ws_tunnel::websocket_proto_name(downstream_tls)));
		stream_expires_never(m_local_socket);
		co_await concurrent_transfer();
		co_return true;
	}

} // namespace proxy
