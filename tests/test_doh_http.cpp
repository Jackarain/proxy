//
// test_doh_http.cpp
// ~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/doh_http.hpp"
#include "proxy/use_awaitable.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/local/connect_pair.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http.hpp>
#include <boost/test/unit_test.hpp>

#include <string>


namespace {

	using namespace proxy;

	namespace beast = boost::beast;
	namespace http = beast::http;
	using local = net::local::stream_protocol;

	BOOST_AUTO_TEST_SUITE(doh_http_suite)

	BOOST_AUTO_TEST_CASE(posts_query_and_reads_ok_body)
	{
		net::io_context ioc;
		local::socket server(ioc);
		local::socket client(ioc);
		net::local::connect_pair(server, client);

		std::string server_host;
		std::string server_target;
		std::string server_body;
		std::string server_content_type;

		net::co_spawn(ioc,
			[&]() -> net::awaitable<void>
			{
				auto& socket = server;
				boost::system::error_code ec;

				beast::flat_buffer buffer;
				http::request<http::string_body> request;
				co_await http::async_read(
					socket, buffer, request, net_awaitable[ec]);

				server_host = std::string(request[http::field::host]);
				server_target = std::string(request.target());
				server_body = request.body();
				server_content_type =
					std::string(request[http::field::content_type]);

				http::response<http::string_body> response{
					http::status::ok, request.version() };
				response.set(http::field::content_type,
					"application/dns-message");
				response.body() = "dns-response-bytes";
				response.prepare_payload();
				co_await http::async_write(
					socket, response, net_awaitable[ec]);

				boost::system::error_code ignore;
				socket.shutdown(local::socket::shutdown_send, ignore);
				co_return;
			}, net::detached);

		bool ok = false;
		int status = -1;
		std::string output;

		net::co_spawn(ioc,
			[&]() -> net::awaitable<void>
			{
				auto& socket = client;

				ok = co_await doh_http_post(socket, "dns.example",
					"/dns-query", std::string("dns-query-bytes"),
					output, &status);
				co_return;
			}, net::detached);

		ioc.run();

		BOOST_TEST(ok);
		BOOST_TEST(status == 200);
		BOOST_TEST(output == "dns-response-bytes");
		BOOST_TEST(server_host == "dns.example");
		BOOST_TEST(server_target == "/dns-query");
		BOOST_TEST(server_body == "dns-query-bytes");
		BOOST_TEST(server_content_type == "application/dns-message");
	}

	BOOST_AUTO_TEST_CASE(non_ok_status_returns_false_but_reports_status)
	{
		net::io_context ioc;
		local::socket server(ioc);
		local::socket client(ioc);
		net::local::connect_pair(server, client);

		net::co_spawn(ioc,
			[&]() -> net::awaitable<void>
			{
				auto& socket = server;
				boost::system::error_code ec;

				beast::flat_buffer buffer;
				http::request<http::string_body> request;
				co_await http::async_read(
					socket, buffer, request, net_awaitable[ec]);

				http::response<http::string_body> response{
					http::status::internal_server_error, request.version() };
				response.body() = "boom";
				response.prepare_payload();
				co_await http::async_write(
					socket, response, net_awaitable[ec]);

				boost::system::error_code ignore;
				socket.shutdown(local::socket::shutdown_send, ignore);
				co_return;
			}, net::detached);

		bool ok = true;
		int status = -1;
		std::string output;

		net::co_spawn(ioc,
			[&]() -> net::awaitable<void>
			{
				auto& socket = client;

				ok = co_await doh_http_post(socket, "dns.example",
					"/dns-query", std::string("q"), output, &status);
				co_return;
			}, net::detached);

		ioc.run();

		BOOST_TEST(!ok);
		BOOST_TEST(status == 500);
		BOOST_TEST(output.empty());
	}

	BOOST_AUTO_TEST_CASE(read_failure_returns_false_with_zero_status)
	{
		net::io_context ioc;
		local::socket server(ioc);
		local::socket client(ioc);
		net::local::connect_pair(server, client);

		// 服务端连接建立后立即关闭, 使客户端读写失败.
		net::co_spawn(ioc,
			[&]() -> net::awaitable<void>
			{
				boost::system::error_code ignore;
				server.shutdown(local::socket::shutdown_both, ignore);
				server.close(ignore);
				co_return;
			}, net::detached);

		bool ok = true;
		int status = -1;
		std::string output;

		net::co_spawn(ioc,
			[&]() -> net::awaitable<void>
			{
				auto& socket = client;

				ok = co_await doh_http_post(socket, "dns.example",
					"/dns-query", std::string("q"), output, &status);
				co_return;
			}, net::detached);

		ioc.run();

		BOOST_TEST(!ok);
		BOOST_TEST(status == 0);
		BOOST_TEST(output.empty());
	}

	BOOST_AUTO_TEST_SUITE_END()

} // namespace
