一个使用现代c++实现的socks4/5协议的实现
<BR>
[![CircleCI](https://dl.circleci.com/status-badge/img/gh/Jackarain/libsocks/tree/master.svg?style=shield)](https://dl.circleci.com/status-badge/redirect/gh/Jackarain/libsocks/tree/master)
[![actions workflow](https://github.com/jackarain/libsocks/actions/workflows/Build.yml/badge.svg)](https://github.com/Jackarain/libsocks/actions)
<BR>
<BR>
=======================================

使用c++20协程，支持socks4/socks4a/socks5的server/client实现.
<BR>
<BR>

### 使用示例程序:
<BR>

```
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
	auto executor = co_await net::this_coro::executor;
	tcp::socket s(executor);

	tcp::endpoint server_addr(
		net::ip::address::from_string("127.0.0.1"),
		1080);

	// tcp socket connect to socks server.
	boost::system::error_code ec;
	co_await s.async_connect(server_addr, asio_util::use_awaitable[ec]);
	if (ec)
	{
		LOG_WARN << "client connect to server: " << ec.message();
		co_return;
	}

	// handshake and tell to socks server target.
	socks::socks_client_option opt;

	opt.target_host = "www.baidu.com";
	opt.target_port = 80;

	opt.proxy_hostname = true;
	opt.username = "jack";
	opt.password = "1111";

	co_await socks::async_socks_handshake(s, opt, asio_util::use_awaitable[ec]);
	if (ec)
	{
		LOG_WARN << "client handshake to server: " << ec.message();
		co_return;
	}

	LOG_DBG << "completed socks handshake.";

	// Next, do some http stuff with socket...

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
```


very easy to use!!!
