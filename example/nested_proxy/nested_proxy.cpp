//
// nested_proxy.cpp
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2022 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//


#include <memory>


#include <boost/asio/io_context.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>


#include "proxy/socks_client.hpp"
#include "proxy/http_proxy_client.hpp"
#include "proxy/proxy_server.hpp"
#include "proxy/logging.hpp"

#include "proxy/use_awaitable.hpp"

namespace net = boost::asio;
using namespace proxy;

using server_ptr = std::shared_ptr<proxy::proxy_server>;

net::awaitable<void> start_proxy_server(net::io_context& ioc, server_ptr& server)
{
	tcp::endpoint socks_listen(
		net::ip::address::from_string("0.0.0.0"),
		10800);

	proxy_server_option opt;

	opt.auth_users_.emplace_back("jack", "1111");

	auto executor = ioc.get_executor();
	server = proxy_server::make(
			executor, socks_listen, opt);
	server->start();

	co_return;
}

net::awaitable<void> start_socks_client()
{
	// nested proxy chain example...

	auto executor = co_await net::this_coro::executor;
	tcp::socket sock{ executor };

	tcp::endpoint server_addr(
		net::ip::address::from_string("127.0.0.1"),
		10800);

	boost::system::error_code ec;

	co_await sock.async_connect(server_addr, net_awaitable[ec]);
	if (ec)
	{
		XLOG_WARN << "client connect to server: " << ec.message();
		co_return;
	}

	proxy::socks_client_option opt1;
	opt1.target_host = "127.0.0.1";
	opt1.target_port = 10800;
	opt1.proxy_hostname = true;
	opt1.username = "jack";
	opt1.password = "1111";

	co_await proxy::async_socks_handshake(sock, opt1, net_awaitable[ec]);
	if (ec)
	{
		XLOG_WARN << "client 1' handshake to server: " << ec.message();
		co_return;
	}

	proxy::http_proxy_client_option opt2;
	opt2.target_host = "www.baidu.com";
	opt2.target_port = 80;
	opt2.username = "jack";
	opt2.password = "1111";

	co_await proxy::async_http_proxy_handshake(sock, opt2, net_awaitable[ec]);
	if (ec)
	{
		XLOG_WARN << "client 2' handshake to server: " << ec.message();
		co_return;
	}

	XLOG_DBG << "completed 2' handshake.";

	co_return;
}

int main()
{
	net::io_context ioc(1);
	server_ptr server;

	net::co_spawn(ioc, start_proxy_server(ioc, server), net::detached);
	net::co_spawn(ioc, start_socks_client(), net::detached);

	ioc.run();
	return 0;
}
