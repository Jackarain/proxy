//
// doh_http.hpp
// ~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef INCLUDE__2026_10_13__DOH_HTTP_HPP
#define INCLUDE__2026_10_13__DOH_HTTP_HPP


#include "proxy/use_awaitable.hpp"

#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http.hpp>
#include <boost/system/error_code.hpp>

#include <string>
#include <string_view>


namespace proxy {

	namespace net = boost::asio;
	namespace beast = boost::beast;
	namespace http = beast::http;           // from <boost/beast/http.hpp>

	// doh_http_post 通过一条已建立的连接 (TLS 流或明文 socket) 发送 DoH
	// (application/dns-message) POST 请求并读取响应; 成功返回 true 并将
	// 响应正文写入 output. status_out 非空时回填 HTTP 响应状态码, 未收到
	// 响应时置 0, 供调用方记录日志.
	template <typename Stream>
	net::awaitable<bool> doh_http_post(
		Stream& stream,
		std::string_view host,
		std::string_view path,
		const std::string& dns_query,
		std::string& output,
		int* status_out = nullptr)
	{
		boost::system::error_code ec;

		if (status_out)
			*status_out = 0;

		// 构造 HTTP POST 请求.
		http::request<http::string_body> doh_req{ http::verb::post, path, 11 };
		doh_req.set(http::field::host, host);
		doh_req.set(http::field::content_type, "application/dns-message");
		doh_req.set(http::field::accept, "application/dns-message");
		doh_req.body() = dns_query;
		doh_req.prepare_payload();

		co_await http::async_write(stream, doh_req, net_awaitable[ec]);
		if (ec)
			co_return false;

		beast::flat_buffer buf;
		http::response<http::string_body> doh_res;
		co_await http::async_read(stream, buf, doh_res, net_awaitable[ec]);
		if (ec)
			co_return false;

		if (status_out)
			*status_out = doh_res.result_int();

		if (doh_res.result() != http::status::ok)
			co_return false;

		output = std::move(doh_res.body());
		co_return true;
	}

}


#endif // INCLUDE__2026_10_13__DOH_HTTP_HPP
