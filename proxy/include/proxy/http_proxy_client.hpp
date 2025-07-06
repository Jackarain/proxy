//
// http_proxy_client.hpp
// ~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef INCLUDE__2023_10_18__HTTP_PROXY_CLIENT_HPP
#define INCLUDE__2023_10_18__HTTP_PROXY_CLIENT_HPP


#include "proxy/use_awaitable.hpp"
#include "proxy/strutil.hpp"

#include <boost/system/error_code.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>

#ifdef _MSC_VER
# pragma warning(push)
# pragma warning(disable: 4702)
#endif // _MSC_VER

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

#ifdef _MSC_VER
# pragma warning(pop)
#endif

#include <cstdlib>
#include <string>
#include <memory>


namespace proxy {

	namespace net = boost::asio;

	using tcp = net::ip::tcp;               // from <boost/asio/ip/tcp.hpp>
	using udp = net::ip::udp;               // from <boost/asio/ip/udp.hpp>

	namespace beast = boost::beast;			// from <boost/beast.hpp>
	namespace http = beast::http;           // from <boost/beast/http.hpp>

	// Options for the HTTP proxy client
	struct http_proxy_client_option
	{
		std::string target_host;  // Target server host
		uint16_t target_port;     // Target server port
		std::string username;     // User authentication - username
		std::string password;     // User authentication - password
	};

	namespace detail {

		using http_request = http::request<http::string_body>;
		using http_response = http::response<http::dynamic_body>;

		template <typename Stream>
		net::awaitable<boost::system::error_code>
		do_http_proxy_handshake(Stream& socket, http_proxy_client_option opt = {})
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
				const auto userinfo = opt.username + ":" + opt.password;
				req.set(http::field::proxy_authorization,
					"Basic " + strutil::base64_encode(userinfo));
			}

			http::serializer<true, http::string_body> sr(req);
			co_await http::async_write_header(socket, sr, net_awaitable[ec]);
			if (ec)
				co_return ec;

			http::response_parser<http_response::body_type> p;
			beast::flat_buffer buffer{ 1024 };

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
				Stream* socket, const http_proxy_client_option& opt) const
			{
				auto executor = net::get_associated_executor(handler);
				net::co_spawn(executor,
					[socket, opt, handler = std::move(handler)]
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

#endif // INCLUDE__2023_10_18__HTTP_PROXY_CLIENT_HPP
