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

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <memory>

namespace net = boost::asio;
using net::ip::tcp;
using namespace socks;

using server_ptr = std::shared_ptr<socks_server>;

std::string socks_userid;
std::string socks_passwd;
std::string socks_next_proxy;
bool socks_next_proxy_ssl = false;
std::string ssl_certificate_dir;

net::awaitable<void> start_socks_server(server_ptr& server)
{
	tcp::endpoint socks_listen(
		net::ip::address::from_string("0.0.0.0"),
		1080);

	socks_server_option opt;

	opt.usrdid_ = socks_userid;
	opt.passwd_ = socks_userid;

	opt.next_proxy_ = socks_next_proxy;
	opt.next_proxy_use_ssl_ = socks_next_proxy_ssl;
	opt.ssl_cert_path_ = ssl_certificate_dir;

	auto executor = co_await net::this_coro::executor;
	server =
		std::make_shared<socks_server>(
			executor, socks_listen, opt);
	server->start();

	co_return;
}

int main(int argc, char** argv)
{
	po::options_description desc("Options");
	desc.add_options()
		("socks_userid", po::value<std::string>(&socks_userid)->default_value("jack")->value_name("userid"), "Socks4/5 auth user id.")
		("socks_passwd", po::value<std::string>(&socks_passwd)->default_value("1111")->value_name("passwd"), "Socks4/5 auth password.")
		("socks_next_proxy", po::value<std::string>(&socks_next_proxy)->default_value("")->value_name(""), "Next socks4/5 proxy. (e.g: socks5://user:passwd@ip:port)")
		("socks_next_proxy_ssl", po::value<bool>(&socks_next_proxy_ssl)->default_value(false, "false")->value_name(""), "Next socks4/5 proxy with ssl.")

		("ssl_certificate_dir", po::value<std::string>(&ssl_certificate_dir)->default_value("")->value_name("path"), "SSL certificate dir.")
	;

	// 解析命令行.
	po::variables_map vm;
	po::store(
		po::command_line_parser(argc, argv)
		.options(desc)
		.style(po::command_line_style::unix_style
			| po::command_line_style::allow_long_disguise)
		.run()
		, vm);
	po::notify(vm);

	// 帮助输出.
	if (vm.count("help") || argc == 1)
	{
		std::cout << desc;
		return EXIT_SUCCESS;
	}

	net::io_context ioc(1);
	server_ptr server;

	net::co_spawn(ioc, start_socks_server(server), net::detached);

	ioc.run();

	return EXIT_SUCCESS;
}
