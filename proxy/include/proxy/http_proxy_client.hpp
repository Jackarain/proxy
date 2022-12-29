//
// socks_client.hpp
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

#include "proxy/use_awaitable.hpp"

#include <cstdlib>
#include <string>
#include <memory>

#include <boost/system/error_code.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/core/detail/base64.hpp>

namespace proxy {

	namespace net = boost::asio;

	using tcp = net::ip::tcp;               // from <boost/asio/ip/tcp.hpp>
	using udp = net::ip::udp;               // from <boost/asio/ip/udp.hpp>

	namespace beast = boost::beast;			// from <boost/beast.hpp>
	namespace http = beast::http;           // from <boost/beast/http.hpp>

	using http_request = http::request<http::string_body>;
	using http_response = http::response<http::dynamic_body>;

	using ssl_stream = net::ssl::stream<tcp::socket>;

	struct http_proxy_client_option
	{
		// target server
		std::string target_host;
		uint16_t target_port;

		// user auth info
		std::string username;
		std::string password;

		bool ssl{ false };
	};

	namespace detail {

		template <typename Stream>
		net::awaitable<boost::system::error_code>
		do_http_proxy_handshake(
			Stream& socket, http_proxy_client_option opt = {})
		{
			boost::system::error_code ec;

			std::string target_host =
				opt.target_host + ":" + std::to_string(opt.target_port);

			http_request req{ beast::http::verb::connect, target_host, 11 };
			req.set(http::field::proxy_connection, "Keep-Alive");
			req.set(http::field::host, target_host);
			req.set(http::field::user_agent, "avpn/1.0");

			if (!opt.username.empty())
			{
				auto userinfo =
					opt.username + ":" + opt.password;

				std::string result(
					beast::detail::base64::encoded_size(userinfo.size()),
					0);

				auto len =
					beast::detail::base64::encode(
					(char*)result.data(),
					userinfo.c_str(),
					userinfo.size());

				result.resize(len);
				result = "Basic " + result;
				req.set(http::field::proxy_authorization, result);
			}

			http::serializer<true, http::string_body> sr(req);
			co_await http::async_write_header(socket, sr, net_awaitable[ec]);
			if (ec)
				co_return ec;

			http::response_parser<
				http_response::body_type> p;
			boost::beast::flat_buffer buffer{ 1024 };

			do {
				co_await http::async_read_header(
					socket, buffer, p, net_awaitable[ec]);
				if (ec)
					co_return ec;
			} while (!p.is_header_done());

			co_return ec;
		}

		struct initiate_do_http_proxy
		{
			template <typename Stream, typename Handler>
			void operator()(Handler&& handler,
				Stream* socket, http_proxy_client_option opt) const
			{
				auto executor = net::get_associated_executor(handler);
				net::co_spawn(executor,
					[socket, opt = opt, handler = std::move(handler)]
				() mutable->net::awaitable<void>
				{
					auto ec = co_await do_http_proxy_handshake(*socket, opt);
					handler(ec);

					co_return;
				}, net::detached);
			}
		};
	}


	template <typename Stream, typename Handler>
	auto async_http_proxy_handshake(Stream& socket,
		http_proxy_client_option opt, Handler&& handler)
	{
		return net::async_initiate<Handler,
			void(boost::system::error_code)>(
				detail::initiate_do_http_proxy(), handler, &socket, opt);
	}
}
