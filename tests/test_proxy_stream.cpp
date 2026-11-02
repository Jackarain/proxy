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
#include <boost/asio/read.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/write.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>


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

	BOOST_AUTO_TEST_CASE(init_proxy_stream_tls_preserves_uds_transport)
	{
		net::io_context ioc;
		local::socket server(ioc);
		local::socket client(ioc);
		net::local::connect_pair(server, client);

		net::ssl::context ctx(net::ssl::context::tls_server);
		auto stream = init_proxy_stream(
			proxy_uds_socket(std::move(server)), ctx);

		BOOST_TEST(boost::variant2::holds_alternative<ssl_uds_stream>(stream));
		BOOST_TEST(!boost::variant2::holds_alternative<ssl_tcp_stream>(stream));
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

	BOOST_AUTO_TEST_CASE(scrambled_write_handles_multiple_buffers)
	{
		net::io_context ioc;
		local::socket server(ioc);
		local::socket client(ioc);
		net::local::connect_pair(server, client);

		proxy_uds_socket writer(std::move(client));
		proxy_uds_socket reader(std::move(server));

		std::array<uint8_t, 16> key{};
		for (size_t i = 0; i < key.size(); ++i)
			key[i] = static_cast<uint8_t>(i + 1);
		writer.set_scramble_key(key);
		reader.set_unscramble_key(key);

		const std::string first = "first-";
		const std::string second = "second-buffer";
		const std::string expected = first + second;
		const std::array<net::const_buffer, 2> buffers{
			net::buffer(first), net::buffer(second) };
		std::array<char, 32> received{};
		boost::system::error_code read_ec;
		boost::system::error_code write_ec;

		net::async_read(reader, net::buffer(received, expected.size()),
			[&](const boost::system::error_code& ec, size_t)
			{
				read_ec = ec;
			});
		net::async_write(writer, buffers,
			[&](const boost::system::error_code& ec, size_t)
			{
				write_ec = ec;
			});

		ioc.run();

		BOOST_TEST(!write_ec);
		BOOST_TEST(!read_ec);
		BOOST_TEST(first == "first-");
		BOOST_TEST(second == "second-buffer");
		BOOST_TEST(std::string(received.data(), expected.size()) == expected);
	}

	BOOST_AUTO_TEST_CASE(scrambled_read_handles_multiple_buffers)
	{
		net::io_context ioc;
		local::socket server(ioc);
		local::socket client(ioc);
		net::local::connect_pair(server, client);

		proxy_uds_socket writer(std::move(client));
		proxy_uds_socket reader(std::move(server));

		std::array<uint8_t, 16> key{};
		for (size_t i = 0; i < key.size(); ++i)
			key[i] = static_cast<uint8_t>(i * 3 + 1);
		writer.set_scramble_key(key);
		reader.set_unscramble_key(key);

		const std::string plain = "multiple-read-buffers";
		const std::string expected = plain;
		std::array<char, 8> first{};
		std::array<char, 13> second{};
		const std::array<net::mutable_buffer, 2> buffers{
			net::buffer(first), net::buffer(second) };
		boost::system::error_code read_ec;
		boost::system::error_code write_ec;

		net::async_read(reader, buffers,
			[&](const boost::system::error_code& ec, size_t)
			{
				read_ec = ec;
			});
		net::async_write(writer, net::buffer(plain),
			[&](const boost::system::error_code& ec, size_t)
			{
				write_ec = ec;
			});

		ioc.run();

		std::string received(first.data(), first.size());
		received.append(second.data(), expected.size() - first.size());
		BOOST_TEST(!write_ec);
		BOOST_TEST(!read_ec);
		BOOST_TEST(plain == expected);
		BOOST_TEST(received == expected);
	}

	BOOST_AUTO_TEST_SUITE_END()

} // namespace
