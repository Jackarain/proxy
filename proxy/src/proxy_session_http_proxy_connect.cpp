//
// proxy_session_http_proxy_connect.cpp
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
	net::awaitable<bool> proxy_session::http_proxy_connect() noexcept
	{
		boost::system::error_code ec;
		http::request_parser<http::string_body> parser;

		parser.header_limit(128 * 1024);

		// 慢速请求防护: CONNECT 请求头读取同样受 tcp_timeout 约束.
		stream_expires_after(m_local_socket, std::chrono::seconds(m_option.tcp_timeout_));

		// 读取 http 请求头.
		co_await http::async_read_header(m_local_socket,
			m_local_buffer, parser, net_awaitable[ec]);
		if (ec)
		{
			log_conn_error()
				<< ", http_proxy_connect async_read: "
				<< ec.message();

			co_return false;
		}

		http::request<http::string_body> req = parser.release();

		auto mth = std::string(req.method_string());
		auto target_view = std::string(req.target());
		auto pa = std::string(req[http::field::proxy_authorization]);

		std::string padding_value;
		std::size_t padding_length = 0;

		// 如果有 padding 头, 则回复一个随机长度的 padding 头, 以迷惑扫描者.
		if (req.find("Proxy-Padding") != req.end())
		{
			auto noise_length = req["Proxy-Padding"].size();
			padding_length = noise_length;

			std::mt19937 gen(global_random_device()());
			std::uniform_int_distribution<std::size_t> dis(16, std::max<std::size_t>(16, noise_length));
			noise_length = dis(gen);

			auto noise = generate_noise(global_random_device(), static_cast<uint16_t>(noise_length));
			auto len = boost::beast::detail::base64::encoded_size(noise_length);
			padding_value.resize(len);
			len = beast::detail::base64::encode(padding_value.data(), noise.data(), noise.size());
			padding_value.resize(len);
		}

		log_conn_debug()
			<< ", method: " << mth
			<< ", target: " << target_view
			<< (pa.empty() ? std::string()
				: ", proxy_authorization: " + pa)
			<< (padding_length > 0 ?
				", in padding: " + std::to_string(padding_length)
				: std::string())
			<< (padding_value.empty() ? std::string() :
				", out padding: " + std::to_string(padding_value.size()));

		// http 代理认证.
		auto auth = co_await http_authorization(pa);
		if (auth != PROXY_AUTH_SUCCESS)
		{
			log_conn_warning()
				<< ", proxy auth error: "
				<< pauth_error_message(auth);

			auto date_string = server_date_string();
			auto fake_page = fmt::vformat(fake_407_content_fmt,
				fmt::make_format_args(date_string));

			co_await net::async_write(
				m_local_socket,
				net::buffer(fake_page),
				net::transfer_all(),
				net_awaitable[ec]);

			co_return true;
		}

		std::string host;
		uint16_t port = 0;
		if (!ws_tunnel::parse_authority(target_view, host, port))
		{
			log_conn_error()
				<< ", illegal target: "
				<< target_view;
			co_return false;
		}

		ec = co_await start_connect_host(host, port, false);
		if (ec)
		{
			log_conn_warning()
				<< ", connect to target "
				<< host
				<< ":"
				<< port
				<< " error: "
				<< ec.message();

			co_return true;
		}

		http::response<http::empty_body> res{
			http::status::ok, req.version() };
		res.reason("Connection established");

		if (!padding_value.empty())
			res.insert("Proxy-Padding", padding_value);

		co_await http::async_write(
			m_local_socket,
			res,
			net_awaitable[ec]);
		if (ec)
		{
			log_conn_warning()
				<< ", write http connect response "
				<< host
				<< ":"
				<< port
				<< " error: "
				<< ec.message();

			co_return false;
		}

		// 清除请求阶段设置的 socket 超时, 隧道期的超时交给 idle_timeout
		// 管理, 否则 Beast 的固定截止时间会因上传方向读挂起而误断活跃连接.
		stream_expires_never(m_local_socket);

		co_await concurrent_transfer();

		co_return true;
	}

} // namespace proxy
