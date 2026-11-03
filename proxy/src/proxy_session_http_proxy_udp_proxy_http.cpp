//
// proxy_session_http_proxy_udp_proxy_http.cpp
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
	net::awaitable<void> proxy_session::http_udp_proxy_pass_http(
		const http::request<http::string_body>& req,
		const std::string& target_host,
		uint16_t target_port,
		const std::string& scheme)
	{
		boost::system::error_code ec;

		// HTTP proxy_pass: 连接上游并转发原始请求.
		tcp::socket remote_socket(m_executor);
		auto targets = co_await resolve_proxy_pass_targets();

		ec = co_await connect_proxy_pass(remote_socket, targets);
		if (ec)
		{
			http::response<http::string_body> res{
				http::status::bad_gateway,
				req.version() };
			res.set(http::field::content_type, "text/html");
			res.body() = fake_502_proxy_connect_content;
			res.prepare_payload();

			co_await http::async_write(
				m_local_socket, res, net_awaitable[ec]);
			co_return;
		}

		// 将 m_remote_socket 初始化为 TCP 流类型.
		const bool use_ssl = m_option.proxy_pass_use_ssl_ ||
			proxy_scheme_use_ssl(scheme);
		m_remote_socket = std::move(
			co_await instantiate_proxy_pass(
				std::move(remote_socket),
				use_ssl));

		// 转发到上游前替换下游代理认证信息.
		auto upstream_req = req;
		upstream_req.erase(http::field::proxy_authorization);
		upstream_req.erase(http::field::proxy_connection);
		if (!m_proxy_pass->user().empty())
		{
			const auto userinfo =
				std::string(m_proxy_pass->user()) + ":" +
				std::string(m_proxy_pass->password());
			upstream_req.set(http::field::proxy_authorization,
				"Basic " + strutil::base64_encode(userinfo));
		}

		// 发送请求头到上游.
		co_await http::async_write(
			m_remote_socket, upstream_req, net_awaitable[ec]);
		if (ec)
		{
			log_conn_warning()
				<< ", connect-udp: forward request to upstream: "
				<< ec.message();
			co_return;
		}

		// 读取上游的响应.
		beast::flat_buffer buf;

		http::response_parser<http::string_body> parser;
		parser.body_limit(1024 * 1024 * 10);

		co_await http::async_read(
			m_remote_socket, buf, parser, net_awaitable[ec]);
		if (ec)
		{
			log_conn_warning()
				<< ", connect-udp: read upstream response: "
				<< ec.message();
			co_return;
		}

		auto upstream_res = parser.release();

		// 把上游响应转发给客户端.
		co_await http::async_write(
			m_local_socket, upstream_res, net_awaitable[ec]);
		if (ec)
		{
			log_conn_warning()
				<< ", connect-udp: forward response to client: "
				<< ec.message();
			co_return;
		}

		// 如果上游返回 101, 则开始原始 TCP 双向转发 (上游负责 capsule ↔ UDP).
		if (upstream_res.result() == http::status::switching_protocols)
		{
			// 响应头之后可能已预读到隧道首包, 交给并发转发前补发.
			if (buf.size() > 0)
			{
				m_remote_buffer.commit(net::buffer_copy(
					m_remote_buffer.prepare(buf.size()), buf.data()));
				buf.consume(buf.size());
			}

			log_conn_debug()
				<< ", connect-udp: tunnel established via upstream, "
				<< "starting raw tcp forwarding";

			// 同上, 隧道建立后清除请求阶段的 socket 超时, 交由 idle_timeout 管理.
			stream_expires_never(m_local_socket);

			co_await concurrent_transfer();
		}

		co_return;
	}

} // namespace proxy
