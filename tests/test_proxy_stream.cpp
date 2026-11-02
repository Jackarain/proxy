//
// test_proxy_stream.cpp
// ~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/proxy_stream.hpp"

#include <boost/test/unit_test.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/local/connect_pair.hpp>
#include <boost/asio/ssl/context.hpp>


namespace {

	namespace net = boost::asio;
	using local = net::local::stream_protocol;
	using namespace util;

	BOOST_AUTO_TEST_SUITE(proxy_stream_suite)

	BOOST_AUTO_TEST_CASE(net_uds_socket_accepts_uds_variants)
	{
		net::io_context ioc;
		local::socket server(ioc);
		local::socket client(ioc);
		net::local::connect_pair(server, client);

		const auto server_handle = server.native_handle();
		proxy_uds_socket plain_socket(std::move(server));
		variant_stream_type plain_variant(std::move(plain_socket));
		BOOST_TEST(net_uds_socket(plain_variant).native_handle() == server_handle);

		const auto client_handle = client.native_handle();
		net::ssl::context ctx(net::ssl::context::tls_client);
		ssl_uds_stream tls_socket(proxy_uds_socket(std::move(client)), ctx);
		variant_stream_type tls_variant(std::move(tls_socket));
		BOOST_TEST(net_uds_socket(tls_variant).native_handle() == client_handle);
	}

	BOOST_AUTO_TEST_SUITE_END()

} // namespace
