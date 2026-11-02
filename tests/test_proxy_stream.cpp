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

#include <filesystem>


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

	BOOST_AUTO_TEST_CASE(uds_remote_endpoint_returns_peer_path)
	{
		const auto server_path = std::filesystem::temp_directory_path() /
			"proxy_unit_uds_server.sock";
		const auto client_path = std::filesystem::temp_directory_path() /
			"proxy_unit_uds_client.sock";

		auto remove_paths = [&]
		{
			std::error_code ec;
			std::filesystem::remove(server_path, ec);
			std::filesystem::remove(client_path, ec);
		};

		remove_paths();

		net::io_context ioc;
		const local::endpoint server_endpoint(server_path.string());
		const local::endpoint client_endpoint(client_path.string());
		boost::system::error_code ec;

		local::acceptor acceptor(ioc);
		acceptor.open(server_endpoint.protocol(), ec);
		BOOST_TEST(!ec);
		acceptor.bind(server_endpoint, ec);
		BOOST_TEST(!ec);
		acceptor.listen(net::socket_base::max_listen_connections, ec);
		BOOST_TEST(!ec);

		local::socket server(ioc);
		local::socket client(ioc);
		boost::system::error_code accept_ec;
		boost::system::error_code connect_ec;

		acceptor.async_accept(server,
			[&accept_ec](const boost::system::error_code& error)
			{
				accept_ec = error;
			});

		client.open(client_endpoint.protocol(), ec);
		BOOST_TEST(!ec);
		client.bind(client_endpoint, ec);
		BOOST_TEST(!ec);
		client.async_connect(server_endpoint,
			[&connect_ec](const boost::system::error_code& error)
			{
				connect_ec = error;
			});

		ioc.run();
		BOOST_TEST(!accept_ec);
		BOOST_TEST(!connect_ec);

		{
			proxy_uds_socket server_socket(std::move(server));
			proxy_uds_socket client_socket(std::move(client));

			BOOST_TEST(uds_remote_endpoint(server_socket).path() ==
				client_path.string());
			BOOST_TEST(uds_remote_endpoint(client_socket).path() ==
				server_path.string());
		}

		remove_paths();
	}

	BOOST_AUTO_TEST_SUITE_END()

} // namespace
