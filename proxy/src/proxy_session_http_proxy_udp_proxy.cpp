//
// proxy_session_http_proxy_udp_proxy.cpp
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

	net::awaitable<void> proxy_session::http_proxy_udp_proxy_pass(
		const http::request<http::string_body>& req,
		const std::string& target_host,
		uint16_t target_port)
	{
		auto scheme = boost::to_lower_copy(std::string(m_proxy_pass->scheme()));

		if (proxy_scheme_is_websocket(scheme))
		{
			co_await http_udp_proxy_pass_websocket(
				req, target_host, target_port);
			co_return;
		}

		if (scheme.starts_with("socks5"))
		{
			co_await http_udp_proxy_pass_socks5(
				req, target_host, target_port, scheme);
			co_return;
		}

		co_await http_udp_proxy_pass_http(
			req, target_host, target_port, scheme);
	}

} // namespace proxy
