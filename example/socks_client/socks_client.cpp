//
// socks_client.cpp
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2022 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/io_context.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

#include <boost/program_options.hpp>

#include "proxy/socks_client.hpp"
#include "proxy/logging.hpp"

#include "proxy/use_awaitable.hpp"


namespace net = boost::asio;
using net::ip::tcp;
using namespace proxy;


net::awaitable<void> start_socks_client()
{
	auto executor = co_await net::this_coro::executor;
	tcp::socket sock(executor);

	tcp::endpoint server_addr(
		net::ip::address::from_string("127.0.0.1"),
		1080);

	boost::system::error_code ec;
	co_await sock.async_connect(server_addr, net_awaitable[ec]);
	if (ec)
	{
		XLOG_WARN << "client connect to server: " << ec.message();
		co_return;
	}

	proxy::socks_client_option opt;
	opt.target_host = "10.0.0.1";
	opt.target_port = 443;
	opt.proxy_hostname = true;
	opt.username = "jack";
	opt.password = "1111";

	co_await proxy::async_socks_handshake(sock, opt, net_awaitable[ec]);
	if (ec)
	{
		XLOG_WARN << "client 1' handshake to server: " << ec.message();
		co_return;
	}

	// Using socket 'sock' do something...

	co_return;
}

int main()
{
	net::io_context ioc(1);

	net::co_spawn(ioc, start_socks_client(), net::detached);

	ioc.run();

	return EXIT_SUCCESS;
}
