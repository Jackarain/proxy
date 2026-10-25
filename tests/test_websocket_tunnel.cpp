//
// test_websocket_tunnel.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/proxy_websocket_client.hpp"
#include "proxy/proxy_stream.hpp"
#include "proxy/proxy_util.hpp"
#include "proxy/websocket_tunnel.hpp"

#include <boost/test/unit_test.hpp>

#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>

#include <chrono>
#include <exception>
#include <string>
#include <vector>


namespace {

	namespace net = boost::asio;
	namespace beast = boost::beast;
	namespace http = boost::beast::http;
	namespace websocket = boost::beast::websocket;
	using tcp = net::ip::tcp;
	using namespace proxy;

	BOOST_AUTO_TEST_SUITE(websocket_tunnel_suite)

	BOOST_AUTO_TEST_CASE(parse_authority)
	{
		std::string host;
		uint16_t port = 0;

		BOOST_TEST(ws_tunnel::parse_authority(
			" target.example.com:443 ", host, port));
		BOOST_TEST(host == "target.example.com");
		BOOST_TEST(port == 443);

		BOOST_TEST(ws_tunnel::parse_authority("127.0.0.1:1", host, port));
		BOOST_TEST(host == "127.0.0.1");
		BOOST_TEST(port == 1);

		BOOST_TEST(ws_tunnel::parse_authority("[2001:db8::1]:65535", host, port));
		BOOST_TEST(host == "2001:db8::1");
		BOOST_TEST(port == 65535);

		BOOST_TEST(!ws_tunnel::parse_authority("example.com", host, port));
		BOOST_TEST(!ws_tunnel::parse_authority("example.com:0", host, port));
		BOOST_TEST(!ws_tunnel::parse_authority("example.com:65536", host, port));
		BOOST_TEST(!ws_tunnel::parse_authority(":443", host, port));
		BOOST_TEST(!ws_tunnel::parse_authority("2001:db8::1:443", host, port));
		BOOST_TEST(ws_tunnel::parse_authority("[::1]:443", host, port));
		BOOST_TEST(host == "::1");
		BOOST_TEST(port == 443);
	}

	BOOST_AUTO_TEST_CASE(format_authority)
	{
		BOOST_TEST(ws_tunnel::format_authority("example.com", 443) ==
			"example.com:443");
		BOOST_TEST(ws_tunnel::format_authority("2001:db8::1", 443) ==
			"[2001:db8::1]:443");
	}

	BOOST_AUTO_TEST_CASE(header_token)
	{
		BOOST_TEST(ws_tunnel::header_has_token(
			"gzip, proxy-connect.v1", "proxy-connect.v1"));
		BOOST_TEST(ws_tunnel::header_has_token(
			"PROXY-CONNECT.V1", "proxy-connect.v1"));
		BOOST_TEST(ws_tunnel::header_has_token(
			" proxy-connect.v1 ", "proxy-connect.v1"));
		BOOST_TEST(!ws_tunnel::header_has_token(
			"proxy-connect.v2", "proxy-connect.v1"));
		BOOST_TEST(!ws_tunnel::header_has_token("", "proxy-connect.v1"));
	}

	BOOST_AUTO_TEST_CASE(proxy_scheme)
	{
		BOOST_TEST(!proxy_scheme_use_ssl("ws"));
		BOOST_TEST(proxy_scheme_use_ssl("wss"));
		BOOST_TEST(proxy_scheme_use_ssl("https"));
		BOOST_TEST(proxy_scheme_use_ssl("socks5s"));
		BOOST_TEST(!proxy_scheme_use_ssl("http"));

		BOOST_TEST(proxy_scheme_is_websocket("ws"));
		BOOST_TEST(proxy_scheme_is_websocket("WSS"));
		BOOST_TEST(!proxy_scheme_is_websocket("http"));
	}

	BOOST_AUTO_TEST_CASE(stream_type_size)
	{
		BOOST_TEST(sizeof(util::websocket_plain_stream) < 4096);
		BOOST_TEST(sizeof(util::websocket_tls_stream) < 4096);
		BOOST_TEST(sizeof(util::ssl_websocket_plain_stream) < 16384);
		BOOST_TEST(sizeof(util::ssl_websocket_tls_stream) < 16384);
		BOOST_TEST(sizeof(util::variant_stream_type) < 65536);
	}

	struct loopback_state
	{
		boost::system::error_code client_ec;
		boost::system::error_code server_ec;
		std::exception_ptr client_exception;
		std::exception_ptr server_exception;
		std::string client_received;
		std::string server_received;
		bool client_eof { false };
		bool server_eof { false };
		bool client_done { false };
		bool server_done { false };
		bool timed_out { false };
	};

	net::awaitable<void> run_server(tcp::acceptor& acceptor,
		loopback_state& state)
	{
		boost::system::error_code ec;
		tcp::socket socket = co_await acceptor.async_accept(
			net_awaitable[ec]);
		if (ec)
		{
			state.server_ec = ec;
			co_return;
		}

		ws_tunnel::stream<util::proxy_tcp_socket> ws(
			util::proxy_tcp_socket(std::move(socket)));
		beast::flat_buffer http_buffer;
		http::request<http::string_body> req;
		co_await http::async_read(ws.next_layer(), http_buffer, req,
			net_awaitable[ec]);
		if (ec)
		{
			state.server_ec = ec;
			co_return;
		}

		ec = co_await ws.async_accept(req, http_buffer.data());
		if (ec)
		{
			state.server_ec = ec;
			co_return;
		}

		char buffer[64]{};
		auto bytes = co_await ws.async_read_some(
			net::buffer(buffer), net_awaitable[ec]);
		if (ec)
		{
			state.server_ec = ec;
			co_return;
		}
		state.server_received.assign(buffer, bytes);

		bytes = co_await ws.async_read_some(
			net::buffer(buffer), net_awaitable[ec]);
		state.server_eof = ec == net::error::eof;
		state.server_ec = state.server_eof ? boost::system::error_code() : ec;
		if (!state.server_eof)
			co_return;

		co_await net::async_write(ws, net::buffer("world", 5),
			net_awaitable[ec]);
		if (ec)
		{
			state.server_ec = ec;
			co_return;
		}

		boost::system::error_code ignore_ec;
		co_await ws.async_shutdown_send(net_awaitable[ignore_ec]);
		state.server_ec = ignore_ec;
	}

	net::awaitable<void> run_client(tcp::endpoint endpoint,
		loopback_state& state)
	{
		boost::system::error_code ec;
		tcp::socket socket(co_await net::this_coro::executor, tcp::v4());
		co_await socket.async_connect(endpoint, net_awaitable[ec]);
		if (ec)
		{
			state.client_ec = ec;
			co_return;
		}

		ws_tunnel::stream<util::proxy_tcp_socket> ws(
			util::proxy_tcp_socket(std::move(socket)));
		ws_tunnel::client_handshake_options options;
		options.host = "127.0.0.1";
		options.target = std::string(ws_tunnel::proxy_connect_path);
		options.authority = "target.example.com:80";
		ec = co_await ws.async_handshake(std::move(options));
		if (ec)
		{
			state.client_ec = ec;
			co_return;
		}

		co_await net::async_write(ws, net::buffer("hello", 5),
			net_awaitable[ec]);
		if (ec)
		{
			state.client_ec = ec;
			co_return;
		}

		boost::system::error_code shutdown_ec;
		co_await ws.async_shutdown_send(net_awaitable[shutdown_ec]);
		if (shutdown_ec)
		{
			state.client_ec = shutdown_ec;
			co_return;
		}

		char buffer[64]{};
		auto bytes = co_await ws.async_read_some(
			net::buffer(buffer), net_awaitable[ec]);
		if (ec)
		{
			state.client_ec = ec;
			co_return;
		}
		state.client_received.assign(buffer, bytes);

		bytes = co_await ws.async_read_some(
			net::buffer(buffer), net_awaitable[ec]);
		state.client_eof = ec == net::error::eof;
		state.client_ec = state.client_eof ? boost::system::error_code() : ec;
	}

	BOOST_AUTO_TEST_CASE(loopback_data_and_fin)
	{
		net::io_context ioc;
		tcp::acceptor acceptor(ioc,
			tcp::endpoint(net::ip::make_address("127.0.0.1"), 0));
		loopback_state state;

		auto finish = [&ioc, &state]
		{
			if (state.client_done && state.server_done)
				ioc.stop();
		};

		net::co_spawn(ioc, run_server(acceptor, state),
			[&](std::exception_ptr ep)
			{
				state.server_exception = ep;
				state.server_done = true;
				if (ep || state.server_ec)
					ioc.stop();
				finish();
			});
		net::co_spawn(ioc, run_client(acceptor.local_endpoint(), state),
			[&](std::exception_ptr ep)
			{
				state.client_exception = ep;
				state.client_done = true;
				if (ep || state.client_ec)
					ioc.stop();
				finish();
			});

		net::steady_timer timeout(ioc);
		timeout.expires_after(std::chrono::seconds(5));
		timeout.async_wait(
			[&](const boost::system::error_code& ec)
			{
				if (!ec)
				{
					state.timed_out = true;
					ioc.stop();
				}
			});

		ioc.run();

		BOOST_TEST(!state.timed_out);
		BOOST_TEST(!state.client_exception);
		BOOST_TEST(!state.server_exception);
		BOOST_TEST(!state.client_ec);
		BOOST_TEST(!state.server_ec);
		BOOST_TEST(state.server_received == "hello");
		BOOST_TEST(state.client_received == "world");
		BOOST_TEST(state.client_eof);
		BOOST_TEST(state.server_eof);
	}

	struct prefix_state
	{
		boost::system::error_code server_ec;
		std::exception_ptr server_exception;
		std::string received;
		bool server_done { false };
		bool timed_out { false };
	};

	net::awaitable<void> run_prefix_server(tcp::acceptor& acceptor,
		prefix_state& state)
	{
		boost::system::error_code ec;
		tcp::socket socket = co_await acceptor.async_accept(
			net_awaitable[ec]);
		if (ec)
		{
			state.server_ec = ec;
			co_return;
		}

		ws_tunnel::stream<util::proxy_tcp_socket> ws(
			util::proxy_tcp_socket(std::move(socket)));
		beast::flat_buffer http_buffer;
		http::request<http::string_body> req;
		co_await http::async_read(ws.next_layer(), http_buffer, req,
			net_awaitable[ec]);
		if (ec)
		{
			state.server_ec = ec;
			co_return;
		}

		ec = co_await ws.async_accept(req, http_buffer.data());
		if (ec)
		{
			state.server_ec = ec;
			co_return;
		}

		char buffer[64]{};
		auto bytes = co_await ws.async_read_some(
			net::buffer(buffer), net_awaitable[ec]);
		state.server_ec = ec;
		if (!ec)
			state.received.assign(buffer, bytes);
	}

	net::awaitable<void> run_prefix_client(tcp::endpoint endpoint)
	{
		boost::system::error_code ec;
		tcp::socket socket(co_await net::this_coro::executor, tcp::v4());
		co_await socket.async_connect(endpoint, net_awaitable[ec]);
		if (ec)
			co_return;

		std::string wire;
		wire.append("GET /.well-known/proxy/connect HTTP/1.1\r\n");
		wire.append("Host: 127.0.0.1\r\n");
		wire.append("Connection: Upgrade\r\n");
		wire.append("Upgrade: websocket\r\n");
		wire.append("Sec-WebSocket-Version: 13\r\n");
		wire.append("Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n");
		wire.append("Sec-WebSocket-Protocol: proxy-connect.v1\r\n");
		wire.append("Proxy-Connect: target.example.com:80\r\n\r\n");

		const std::array<unsigned char, 4> mask{ 1, 2, 3, 4 };
		const std::string payload("\0hello", 6);
		wire.push_back(static_cast<char>(0x82));
		wire.push_back(static_cast<char>(0x80 | payload.size()));
		for (auto byte : mask)
			wire.push_back(static_cast<char>(byte));
		for (std::size_t i = 0; i < payload.size(); ++i)
		{
			wire.push_back(static_cast<char>(
				static_cast<unsigned char>(payload[i]) ^ mask[i % mask.size()]));
		}

		co_await net::async_write(socket, net::buffer(wire),
			net_awaitable[ec]);
	}

	BOOST_AUTO_TEST_CASE(prefix_preserved_before_upgrade_response)
	{
		net::io_context ioc;
		tcp::acceptor acceptor(ioc,
			tcp::endpoint(net::ip::make_address("127.0.0.1"), 0));
		prefix_state state;

		net::co_spawn(ioc, run_prefix_server(acceptor, state),
			[&](std::exception_ptr ep)
			{
				state.server_exception = ep;
				state.server_done = true;
				ioc.stop();
			});
		net::co_spawn(ioc,
			run_prefix_client(acceptor.local_endpoint()), net::detached);

		net::steady_timer timeout(ioc);
		timeout.expires_after(std::chrono::seconds(5));
		timeout.async_wait(
			[&](const boost::system::error_code& ec)
			{
				if (!ec)
				{
					state.timed_out = true;
					ioc.stop();
				}
			});

		ioc.run();

		BOOST_TEST(!state.timed_out);
		BOOST_TEST(state.server_done);
		BOOST_TEST(!state.server_exception);
		BOOST_TEST(!state.server_ec);
		BOOST_TEST(state.received == "hello");
	}

	struct abort_state
	{
		boost::system::error_code server_ec;
		std::exception_ptr server_exception;
		bool stream_closed { false };
		bool timed_out { false };
	};

	net::awaitable<void> run_abort_server(tcp::acceptor& acceptor,
		abort_state& state)
	{
		boost::system::error_code ec;
		tcp::socket socket = co_await acceptor.async_accept(
			net_awaitable[ec]);
		if (ec)
		{
			state.server_ec = ec;
			co_return;
		}

		ws_tunnel::stream<util::proxy_tcp_socket> ws(
			util::proxy_tcp_socket(std::move(socket)));
		beast::flat_buffer http_buffer;
		http::request<http::string_body> req;
		co_await http::async_read(ws.next_layer(), http_buffer, req,
			net_awaitable[ec]);
		if (ec)
		{
			state.server_ec = ec;
			co_return;
		}

		ec = co_await ws.async_accept(req, http_buffer.data());
		if (ec)
		{
			state.server_ec = ec;
			co_return;
		}

		char buffer[64]{};
		co_await ws.async_read_some(net::buffer(buffer), net_awaitable[ec]);
		state.server_ec = ec;
		state.stream_closed = !ws.is_open();
	}

	net::awaitable<void> run_abort_client(tcp::endpoint endpoint)
	{
		boost::system::error_code ec;
		tcp::socket socket(co_await net::this_coro::executor, tcp::v4());
		co_await socket.async_connect(endpoint, net_awaitable[ec]);
		if (ec)
			co_return;

		std::string wire;
		wire.append("GET /.well-known/proxy/connect HTTP/1.1\r\n");
		wire.append("Host: 127.0.0.1\r\n");
		wire.append("Connection: Upgrade\r\n");
		wire.append("Upgrade: websocket\r\n");
		wire.append("Sec-WebSocket-Version: 13\r\n");
		wire.append("Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n");
		wire.append("Sec-WebSocket-Protocol: proxy-connect.v1\r\n");
		wire.append("Proxy-Connect: target.example.com:80\r\n\r\n");
		wire.push_back(static_cast<char>(0x82));
		wire.push_back(static_cast<char>(0x80 | 1));
		wire.append("\x01\x02\x03\x04", 4);
		wire.push_back(static_cast<char>(ws_tunnel::opcode_abort ^ 0x01));

		co_await net::async_write(socket, net::buffer(wire),
			net_awaitable[ec]);
	}

	BOOST_AUTO_TEST_CASE(abort_closes_stream_immediately)
	{
		net::io_context ioc;
		tcp::acceptor acceptor(ioc,
			tcp::endpoint(net::ip::make_address("127.0.0.1"), 0));
		abort_state state;

		net::co_spawn(ioc, run_abort_server(acceptor, state),
			[&](std::exception_ptr ep)
			{
				state.server_exception = ep;
				ioc.stop();
			});
		net::co_spawn(ioc,
			run_abort_client(acceptor.local_endpoint()), net::detached);

		net::steady_timer timeout(ioc);
		timeout.expires_after(std::chrono::seconds(5));
		timeout.async_wait(
			[&](const boost::system::error_code& ec)
			{
				if (!ec)
				{
					state.timed_out = true;
					ioc.stop();
				}
			});

		ioc.run();

		BOOST_TEST(!state.timed_out);
		BOOST_TEST(!state.server_exception);
		BOOST_TEST(state.server_ec == net::error::connection_aborted);
		BOOST_TEST(state.stream_closed);
	}

	BOOST_AUTO_TEST_CASE(websocket_udp_handshake_options)
	{
		urls::url url = urls::parse_uri(
			"ws://user:pass@example.com:8080").value();
		auto options = make_websocket_handshake_options(
			url, "127.0.0.1", 53, true);

		BOOST_TEST(options.datagram);
		BOOST_TEST(options.target == ws_tunnel::proxy_connect_udp_path);
		BOOST_TEST(options.authority == "127.0.0.1:53");
		BOOST_TEST(options.username == "user");
		BOOST_TEST(options.password == "pass");
	}

	struct udp_loopback_state
	{
		boost::system::error_code client_ec;
		boost::system::error_code server_ec;
		std::exception_ptr client_exception;
		std::exception_ptr server_exception;
		std::string request_header;
		std::string request_protocol;
		std::string server_payload;
		std::string client_payload;
		bool client_done { false };
		bool server_done { false };
		bool timed_out { false };
	};

	net::awaitable<void> run_udp_server(tcp::acceptor& acceptor,
		udp_loopback_state& state)
	{
		boost::system::error_code ec;
		tcp::socket socket = co_await acceptor.async_accept(
			net_awaitable[ec]);
		if (ec)
		{
			state.server_ec = ec;
			co_return;
		}

		ws_tunnel::stream<util::proxy_tcp_socket> ws(
			util::proxy_tcp_socket(std::move(socket)));
		beast::flat_buffer http_buffer;
		http::request<http::string_body> req;
		co_await http::async_read(ws.next_layer(), http_buffer, req,
			net_awaitable[ec]);
		if (ec)
		{
			state.server_ec = ec;
			co_return;
		}

		auto target = req.find(ws_tunnel::proxy_connect_udp_header);
		if (target != req.end())
			state.request_header = std::string(target->value());
		state.request_protocol =
			std::string(req[http::field::sec_websocket_protocol]);

		ec = co_await ws.async_accept(req, http_buffer.data(),
			ws_tunnel::proxy_connect_udp_subprotocol);
		if (ec)
		{
			state.server_ec = ec;
			co_return;
		}

		beast::flat_buffer message;
		ec = co_await ws.async_read_binary(message);
		if (ec)
		{
			state.server_ec = ec;
			co_return;
		}

		auto data = message.data();
		std::string_view payload;
		BOOST_REQUIRE(decode_datagram_message(std::string_view(
			static_cast<const char*>(data.data()), data.size()), payload));
		state.server_payload.assign(payload);

		const std::string reply = "udp-reply";
		std::vector<uint8_t> capsule(
			datagram_capsule_header_size(reply.size()) + reply.size());
		auto size = encode_datagram_capsule(reply.data(), reply.size(),
			capsule.data(), capsule.size());
		ec = co_await ws.async_write_binary(
			net::buffer(capsule.data(), size));
		if (ec)
			state.server_ec = ec;
	}

	net::awaitable<void> run_udp_client(tcp::endpoint endpoint,
		udp_loopback_state& state)
	{
		boost::system::error_code ec;
		tcp::socket socket(co_await net::this_coro::executor, tcp::v4());
		co_await socket.async_connect(endpoint, net_awaitable[ec]);
		if (ec)
		{
			state.client_ec = ec;
			co_return;
		}

		ws_tunnel::stream<util::proxy_tcp_socket> ws(
			util::proxy_tcp_socket(std::move(socket)));
		ws_tunnel::client_handshake_options options;
		options.host = "127.0.0.1";
		options.target = std::string(ws_tunnel::proxy_connect_udp_path);
		options.authority = "127.0.0.1:53";
		options.datagram = true;
		ec = co_await ws.async_handshake(std::move(options));
		if (ec)
		{
			state.client_ec = ec;
			co_return;
		}

		std::string payload(65000, 'x');
		std::vector<uint8_t> capsule(
			datagram_capsule_header_size(payload.size()) + payload.size());
		auto size = encode_datagram_capsule(payload.data(), payload.size(),
			capsule.data(), capsule.size());
		ec = co_await ws.async_write_binary(
			net::buffer(capsule.data(), size));
		if (ec)
		{
			state.client_ec = ec;
			co_return;
		}

		beast::flat_buffer message;
		ec = co_await ws.async_read_binary(message);
		if (ec)
		{
			state.client_ec = ec;
			co_return;
		}

		auto data = message.data();
		std::string_view reply;
		BOOST_REQUIRE(decode_datagram_message(std::string_view(
			static_cast<const char*>(data.data()), data.size()), reply));
		state.client_payload.assign(reply);
	}

	BOOST_AUTO_TEST_CASE(websocket_udp_datagram_round_trip)
	{
		net::io_context ioc;
		tcp::acceptor acceptor(ioc,
			tcp::endpoint(net::ip::make_address("127.0.0.1"), 0));
		udp_loopback_state state;

		auto finish = [&ioc, &state]
		{
			if (state.client_done && state.server_done)
				ioc.stop();
		};

		net::co_spawn(ioc, run_udp_server(acceptor, state),
			[&](std::exception_ptr ep)
			{
				state.server_exception = ep;
				state.server_done = true;
				if (ep || state.server_ec)
					ioc.stop();
				finish();
			});
		net::co_spawn(ioc,
			run_udp_client(acceptor.local_endpoint(), state),
			[&](std::exception_ptr ep)
			{
				state.client_exception = ep;
				state.client_done = true;
				if (ep || state.client_ec)
					ioc.stop();
				finish();
			});

		net::steady_timer timeout(ioc);
		timeout.expires_after(std::chrono::seconds(5));
		timeout.async_wait(
			[&](const boost::system::error_code& ec)
			{
				if (!ec)
				{
					state.timed_out = true;
					ioc.stop();
				}
			});

		ioc.run();

		BOOST_TEST(!state.timed_out);
		BOOST_TEST(!state.client_exception);
		BOOST_TEST(!state.server_exception);
		BOOST_TEST(!state.client_ec);
		BOOST_TEST(!state.server_ec);
		BOOST_TEST(state.request_header == "127.0.0.1:53");
		BOOST_TEST(state.request_protocol ==
			ws_tunnel::proxy_connect_udp_subprotocol);
		BOOST_TEST(state.server_payload.size() == 65000u);
		BOOST_TEST(state.client_payload == "udp-reply");
	}

	BOOST_AUTO_TEST_SUITE_END()

} // namespace
