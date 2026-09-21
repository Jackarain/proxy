//
// http_get.cpp
// ~~~~~~~~~~~~
//
// 简单示例: 下载 URL 并输出响应体.
//

#include "httpc/httpc.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/detached.hpp>

#include <cstdlib>
#include <iostream>
#include <string>

namespace net = boost::asio;

net::awaitable<void> do_get(std::string url)
{
	httpc::http_client client(co_await net::this_coro::executor);
	client.max_redirects(5);
	client.user_agent("httpc-example/1.0");

	auto url_result = boost::urls::parse_uri_reference(url);
	if (url_result)
		client.set_sni(std::string(url_result->host()));

	client.set_transfer_handler(
		[](void* data, std::size_t size) -> int
		{
			std::cout.write(static_cast<const char*>(data),
				static_cast<std::streamsize>(size));
			return 0;
		});

	httpc::http_request req;
	req.method(httpc::verb::get);

	auto result = co_await client.async_perform(url, req);
	if (!result)
	{
		std::cerr << "request failed: " << result.error().message() << '\n';
		co_return;
	}

	std::cout << "\nstatus: " << result->result_int() << '\n';
}

int main(int argc, char* argv[])
{
	if (argc < 2)
	{
		std::cerr << "usage: " << argv[0] << " <url>\n";
		return EXIT_FAILURE;
	}

	net::io_context ioc;
	net::co_spawn(ioc, do_get(argv[1]), net::detached);
	ioc.run();

	return EXIT_SUCCESS;
}
