一个使用现代c++实现的socks4/5协议的实现
<BR>
[![CircleCI](https://dl.circleci.com/status-badge/img/gh/Jackarain/libsocks/tree/master.svg?style=shield)](https://dl.circleci.com/status-badge/redirect/gh/Jackarain/libsocks/tree/master)
[![actions workflow](https://github.com/jackarain/libsocks/actions/workflows/Build.yml/badge.svg)](https://github.com/Jackarain/libsocks/actions)
<BR>
<BR>
=======================================

使用c++20协程，支持socks4/socks4a/socks5的server/client实现，client与server之间可通过ssl加密通信，可配置如下架构：

<BR>

~~~

                  +--------------+     |      +--------------+
  browser/app --> | socks server | ---ssl---> | socks server | --> target server
                  +--------------+     |      +--------------+
                                      GFW
~~~

同时socks server能伪装为web server来回应非socks协议的请求。

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
	server->start();

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
```
<BR>

有任何问题可加tg账号: https://t.me/jackarain 或tg群组: https://t.me/joinchat/C3WytT4RMvJ4lqxiJiIVhg
