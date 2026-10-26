//
// test_http_proxy_client.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/http_proxy_client.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/write.hpp>
#include <boost/test/unit_test.hpp>

#include <string>


namespace {

	using namespace proxy;

	BOOST_AUTO_TEST_SUITE(http_proxy_client_suite)

	BOOST_AUTO_TEST_CASE(handshake_preserves_buffered_tunnel_data)
	{
		net::io_context ioc;
		tcp::acceptor acceptor(ioc,
			tcp::endpoint(net::ip::make_address("127.0.0.1"), 0));
		tcp::socket client(ioc);
		tcp::socket server(ioc);

		const std::string tunnel_data = "early target data";
		beast::flat_buffer leftover;
		boost::system::error_code result;
		bool finished = false;

		net::co_spawn(ioc,
			[&]() -> net::awaitable<void>
			{
				boost::system::error_code ec;
				co_await acceptor.async_accept(server, net_awaitable[ec]);
				if (ec)
					co_return;

				beast::flat_buffer request_buf;
				http::request_parser<http::empty_body> parser;
				co_await http::async_read(
					server, request_buf, parser, net_awaitable[ec]);
				if (ec)
					co_return;

				const std::string response =
					"HTTP/1.1 200 Connection established\r\n\r\n" + tunnel_data;
				co_await net::async_write(
					server, net::buffer(response), net_awaitable[ec]);
			}, net::detached);

		net::co_spawn(ioc,
			[&]() -> net::awaitable<void>
			{
				boost::system::error_code ec;
				co_await client.async_connect(
					acceptor.local_endpoint(), net_awaitable[ec]);
				if (ec)
				{
					result = ec;
					finished = true;
					co_return;
				}

				http_proxy_client_option opt;
				opt.target_host = "example.com";
				opt.target_port = 443;
				result = co_await detail::do_http_proxy_handshake(
					client, opt, &leftover);
				finished = true;
			}, net::detached);

		ioc.run();

		BOOST_TEST(finished);
		BOOST_TEST(!result);
		BOOST_TEST(leftover.size() == tunnel_data.size());
		BOOST_TEST(std::string(static_cast<const char*>(leftover.data().data()),
			leftover.size()) == tunnel_data);
	}

	BOOST_AUTO_TEST_SUITE_END()

} // namespace
