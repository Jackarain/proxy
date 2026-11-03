//
// proxy_session_http_proxy_udp.cpp
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

	net::awaitable<void> proxy_session::http_proxy_connect_udp(
		const http::request<http::string_body>& req,
		const std::string& target_host,
		uint16_t target_port) noexcept
	{
		boost::system::error_code ec;

		// 如果禁用了 UDP 代理服务, 则拒绝 connect-udp 请求.
		if (m_option.disable_udp_)
		{
			log_conn_warning()
				<< ", connect-udp: udp protocol disabled";

			http::response<http::string_body> res{
				http::status::forbidden, req.version()};
			res.set(http::field::content_type, "text/html");
			res.body() = fake_403_forbidden_content;
			res.prepare_payload();

			co_await http::async_write(
				m_local_socket, res, net_awaitable[ec]);
			co_return;
		}

		// 如果指定了 proxy_pass（有下级代理服务器）.
		if (m_proxy_pass)
		{
			co_await http_proxy_udp_proxy_pass(req, target_host, target_port);
			co_return;
		}
		co_await http_connect_udp_direct(req, target_host, target_port);
		co_return;
	}

} // namespace proxy
