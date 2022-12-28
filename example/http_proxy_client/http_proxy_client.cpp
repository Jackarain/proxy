//
// socks_client.cpp
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2022 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "socks/http_proxy_client.hpp"
#include "socks/logging.hpp"

#include "socks/use_awaitable.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

#include <boost/program_options.hpp>
namespace po = boost::program_options;

namespace net = boost::asio;
using net::ip::tcp;
using namespace socks;


net::awaitable<void> start_http_proxy_client()
{
	auto executor = co_await net::this_coro::executor;
	tcp::socket sock(executor);

	tcp::endpoint server_addr(
		net::ip::address::from_string("10.0.0.1"),
		443);

	boost::system::error_code ec;
	co_await sock.async_connect(server_addr, net_awaitable[ec]);
	if (ec)
	{
		LOG_WARN << "client connect to server: " << ec.message();
		co_return;
	}

	socks::http_proxy_client_option opt;
	opt.target_host = "api.ipify.org";
	opt.target_port = 443;
	opt.username = "jack";
	opt.password = "1111";

	co_await socks::async_http_proxy_handshake(sock, opt, net_awaitable[ec]);
	if (ec)
	{
		LOG_WARN << "client 1' handshake to server: " << ec.message();
		co_return;
	}

	// Usg socket 'sock' do something...

	co_return;
}

int main()
{
	net::io_context ioc(1);

	net::co_spawn(ioc, start_http_proxy_client(), net::detached);

	ioc.run();

	return EXIT_SUCCESS;
}
