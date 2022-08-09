//
// main.cpp
// ~~~~~~~~
//
// Copyright (c) 2022 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "socks/logging.hpp"
#include "socks/socks_server.hpp"
#include "socks/socks_client.hpp"

#include "socks/use_awaitable.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

#include <memory>

namespace net = boost::asio;
using net::ip::tcp;
using namespace socks;

using server_ptr = std::shared_ptr<socks_server>;

net::awaitable<void> start_socks_server(server_ptr& server)
{
	tcp::endpoint socks_listen(
		net::ip::address::from_string("0.0.0.0"),
		1080);

	socks_server_option opt;
	opt.usrdid_ = "jack";
	opt.passwd_ = "1111";

	auto executor = co_await net::this_coro::executor;
	server =
		std::make_shared<socks_server>(
			executor, socks_listen, opt);
	server->open();

	co_return;
}

net::awaitable<void> start_socks_client()
{
	// nested proxy chain example...

	auto executor = co_await net::this_coro::executor;
	tcp::socket s(executor);

	tcp::endpoint server_addr(
		net::ip::address::from_string("127.0.0.1"),
		1080);

	boost::system::error_code ec;
	co_await s.async_connect(server_addr, asio_util::use_awaitable[ec]);
	if (ec)
	{
		LOG_WARN << "client connect to server: " << ec.message();
		co_return;
	}

	socks::socks_client_option opt1;
	opt1.target_host = "127.0.0.1";
	opt1.target_port = 1080;
	opt1.proxy_hostname = true;
	opt1.username = "jack";
	opt1.password = "1111";

	co_await socks::async_socks_handshake(s, opt1, asio_util::use_awaitable[ec]);
	if (ec)
	{
		LOG_WARN << "client 1' handshake to server: " << ec.message();
		co_return;
	}

	socks::socks_client_option opt2;
	opt2.target_host = "www.baidu.com";
	opt2.target_port = 80;
	opt2.proxy_hostname = true;
	opt2.username = "jack";
	opt2.password = "1111";

	co_await socks::async_socks_handshake(s, opt2, asio_util::use_awaitable[ec]);
	if (ec)
	{
		LOG_WARN << "client 2' handshake to server: " << ec.message();
		co_return;
	}

	LOG_DBG << "completed 2' handshake.";

	co_return;
}

int main()
{
	net::io_context ioc(1);
	server_ptr server;

	co_spawn(ioc, start_socks_server(server), net::detached);
	co_spawn(ioc, start_socks_client(), net::detached);

	ioc.run();
	return 0;
}
