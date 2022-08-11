//
// socks_server.cpp
// ~~~~~~~~~~~~~~~~
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

int main()
{
	net::io_context ioc(1);
	server_ptr server;

	net::co_spawn(ioc, start_socks_server(server), net::detached);

	ioc.run();
	return 0;
}
