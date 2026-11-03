//
// proxy_session_http_proxy_websocket_udp.cpp
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

	net::awaitable<std::optional<udp::socket>> proxy_session::open_udp_target(
		const std::string& target_host, uint16_t target_port) noexcept
	{
		boost::system::error_code ec;
		net::ip::address target_addr;
		udp protocol = udp::v4();

		target_addr = net::ip::make_address(target_host, ec);
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
					<< ", websocket udp: DNS resolve failed for "
					<< target_host << ": " << ec.message();
				co_return std::nullopt;
			}

			target_addr = results.begin()->endpoint().address();
		}

		protocol = target_addr.is_v4() ? udp::v4() : udp::v6();
		udp::socket socket(m_executor);
		socket.open(protocol, ec);
		if (ec)
		{
			log_conn_warning()
				<< ", websocket udp: open socket: " << ec.message();
			co_return std::nullopt;
		}

		if (m_bind_interface)
		{
			socket.bind(udp::endpoint(*m_bind_interface, 0), ec);
			if (ec)
			{
				log_conn_warning()
					<< ", websocket udp: bind socket: " << ec.message();
				co_return std::nullopt;
			}
		}

		if (m_option.so_mark_)
		{
			auto ret = apply_so_mark(socket.native_handle(), m_option.so_mark_);
			if (ret.has_error())
			{
				log_conn_warning()
					<< ", websocket udp: set socket mark: "
					<< ret.error().message();
			}
		}

		socket.connect(udp::endpoint(target_addr, target_port), ec);
		if (ec)
		{
			log_conn_warning()
				<< ", websocket udp: connect "
				<< target_host << ":" << target_port
				<< ": " << ec.message();
			co_return std::nullopt;
		}

		co_return std::optional<udp::socket>(std::move(socket));
	}

	net::awaitable<bool> proxy_session::connect_websocket_udp_proxy_pass(
		const std::string& target_host, uint16_t target_port) noexcept
	{
		boost::system::error_code ec;
		tcp::socket remote_socket(m_executor);
		auto targets = co_await resolve_proxy_pass_targets();
		ec = co_await connect_proxy_pass(remote_socket, targets);
		if (ec)
		{
			log_conn_warning()
				<< ", websocket udp: connect proxy_pass: " << ec.message();
			co_return false;
		}

		bool use_ssl = m_option.proxy_pass_use_ssl_ ||
			proxy_scheme_use_ssl(m_proxy_pass->scheme());
		m_remote_socket = co_await instantiate_proxy_pass(
			std::move(remote_socket), use_ssl);

		auto options = make_websocket_handshake_options(
			*m_proxy_pass, target_host, target_port, true);
		ec = co_await async_websocket_proxy_handshake(
			m_remote_socket, std::move(options));
		if (ec)
		{
			log_conn_warning()
				<< ", websocket udp: upstream handshake: " << ec.message();
			stream_cancel(m_remote_socket);
			co_return false;
		}

		stream_expires_never(m_remote_socket);
		co_return true;
	}

	net::awaitable<boost::system::error_code>
	proxy_session::accept_websocket_udp(const string_request& req) noexcept
	{
		if (boost::variant2::holds_alternative<proxy_tcp_socket>(
			m_local_socket))
		{
			websocket_plain_stream websocket(std::move(
				boost::variant2::get<proxy_tcp_socket>(m_local_socket)));
			m_local_socket = init_proxy_stream(std::move(websocket));
			auto buffered = m_local_buffer.size();
			auto ec = co_await boost::variant2::get<websocket_plain_stream>(
				m_local_socket).async_accept(req, m_local_buffer.data(),
					ws_tunnel::proxy_connect_udp_subprotocol);
			if (!ec)
				m_local_buffer.consume(buffered);
			co_return ec;
		}

		if (boost::variant2::holds_alternative<ssl_tcp_stream>(m_local_socket))
		{
			websocket_tls_stream websocket(std::move(
				boost::variant2::get<ssl_tcp_stream>(m_local_socket)));
			m_local_socket = init_proxy_stream(std::move(websocket));
			auto buffered = m_local_buffer.size();
			auto ec = co_await boost::variant2::get<websocket_tls_stream>(
				m_local_socket).async_accept(req, m_local_buffer.data(),
					ws_tunnel::proxy_connect_udp_subprotocol);
			if (!ec)
				m_local_buffer.consume(buffered);
			co_return ec;
		}

		co_return make_error_code(boost::system::errc::operation_not_supported);
	}

} // namespace proxy
