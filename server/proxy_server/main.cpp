//
// main.cpp
// ~~~~~~~~
//
// Copyright (c) 2023 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//


#include <boost/asio/io_context.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

#include <boost/algorithm/string/join.hpp>
#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <filesystem>
namespace fs = std::filesystem;

#include "proxy/proxy_server.hpp"
#include "proxy/socks_client.hpp"
#include "proxy/logging.hpp"

#include "proxy/use_awaitable.hpp"

#include "main.hpp"

namespace net = boost::asio;
using namespace proxy;

using server_ptr = std::shared_ptr<proxy_server>;


//////////////////////////////////////////////////////////////////////////

std::vector<std::string> auth_users;
std::string proxy_pass;
bool proxy_pass_ssl = false;
bool ssl_prefer_server_ciphers = false;
std::string ssl_certificate_dir;

std::string ssl_certificate;
std::string ssl_certificate_key;
std::string ssl_certificate_passwd;
std::string ssl_dhparam;
std::string ssl_sni;

std::string ssl_ciphers;
std::string server_listen;
std::string doc_directory;
std::string log_directory;

bool disable_http = false;
bool disable_socks = false;
bool disable_logs;

bool reuse_port = false;
bool happyeyeballs = true;
std::string local_ip;

//////////////////////////////////////////////////////////////////////////

net::awaitable<void>
start_proxy_server(net::io_context& ioc, server_ptr& server)
{
	std::string host, port;
	bool v6only = false;

	if (!parse_endpoint_string(server_listen, host, port, v6only))
	{
		std::cerr << "Parse endpoint fail: " << server_listen << std::endl;
		co_return;
	}

	tcp::endpoint listen(
		net::ip::address::from_string(host),
			(unsigned short)atoi(port.c_str()));

	proxy_server_option opt;

	for (const auto& user : auth_users)
	{
		if (user.empty())
			continue;

		auto pos = user.find(':');
		if (pos == std::string::npos)
			opt.auth_users_.emplace_back(user, "");
		else
			opt.auth_users_.emplace_back(
				user.substr(0, pos), user.substr(pos + 1));
	}

	opt.proxy_pass_ = proxy_pass;
	opt.proxy_pass_use_ssl_ = proxy_pass_ssl;

	opt.ssl_cert_path_ = ssl_certificate_dir;
	opt.ssl_ciphers_ = ssl_ciphers;
	opt.ssl_certificate_ = ssl_certificate;
	opt.ssl_certificate_key_ = ssl_certificate_key;
	opt.ssl_certificate_passwd_ = ssl_certificate_passwd;
	opt.ssl_dhparam_ = ssl_dhparam;
	opt.ssl_sni_ = ssl_sni;

	opt.disable_http_ = disable_http;
	opt.disable_socks_ = disable_socks;
	opt.local_ip_ = local_ip;

	opt.reuse_port_ = reuse_port;
	opt.happyeyeballs_ = happyeyeballs;

	opt.doc_directory_ = doc_directory;

	server = proxy_server::make(
		ioc.get_executor(), listen, opt);
	server->start();

	co_return;
}

//////////////////////////////////////////////////////////////////////////

inline std::optional<std::string> try_as_string(const boost::any& var)
{
	try {
		return boost::any_cast<std::string>(var);
	}
	catch (const boost::bad_any_cast&) {
		return std::nullopt;
	}
}

inline std::optional<bool> try_as_bool(const boost::any& var)
{
	try {
		return boost::any_cast<bool>(var);
	}
	catch (const boost::bad_any_cast&) {
		return std::nullopt;
	}
}

inline std::optional<int> try_as_int(const boost::any& var)
{
	try {
		return boost::any_cast<int>(var);
	}
	catch (const boost::bad_any_cast&) {
		return std::nullopt;
	}
}


inline void print_args(int argc, char** argv, const po::variables_map& vm)
{
	LOG_INFO << "Current directory: "
		<< fs::current_path().string();

	if (!vm.count("config"))
	{
		std::vector<std::string> args(argv, argv + argc);
		LOG_INFO << "Run: " << boost::algorithm::join(args, " ");
		return;
	}

	for (const auto& [key, value] : vm)
	{
		if (value.empty() || key == "config")
			continue;

		if (auto s = try_as_string(value.value()))
		{
			LOG_INFO << key << " = " << *s;
			continue;
		}

		if (auto b = try_as_bool(value.value()))
		{
			LOG_INFO << key << " = " << *b;
			continue;
		}

		if (auto i = try_as_int(value.value()))
		{
			LOG_INFO << key << " = " << *i;
			continue;
		}
	}
}

namespace std
{
	std::ostream& operator<<(std::ostream &os, const std::vector<std::string> &users)
	{
		for (auto it = users.begin(); it != users.end();)
		{
			os << *it;
			if (++it == users.end())
				break;
			os << " ";
		}

		return os;
	}
}

int main(int argc, char** argv)
{
	platform_init();

	std::string config;

	po::options_description desc("Options");
	desc.add_options()
		("help,h", "Help message.")
		("config", po::value<std::string>(&config)->value_name("config.conf"), "Load configuration options from specified file.")

		("server_listen", po::value<std::string>(&server_listen)->default_value("[::0]:1080")->value_name("ip:port"), "Specify server listening address and port.")

		("reuse_port", po::value<bool>(&reuse_port)->default_value(false), "Enable TCP SO_REUSEPORT option (available since Linux 3.9).")
		("happyeyeballs", po::value<bool>(&happyeyeballs)->default_value(true), "Enable Happy Eyeballs algorithm for TCP connections.")
		("local_ip", po::value<std::string>(&local_ip), "Specify local IP for client TCP connection to server.")

		("auth_users", po::value<std::vector<std::string>>(&auth_users)->multitoken()->default_value(std::vector<std::string>{"jack:1111"}), "List of authorized users (e.g: user1:passwd1 user2:passwd2).")

		("proxy_pass", po::value<std::string>(&proxy_pass)->default_value("")->value_name(""), "Specify next proxy pass (e.g: socks5://user:passwd@ip:port).")
		("proxy_pass_ssl", po::value<bool>(&proxy_pass_ssl)->default_value(false, "false")->value_name(""), "Enable SSL for the next proxy pass.")

		("ssl_certificate_dir", po::value<std::string>(&ssl_certificate_dir)->value_name("path"), "Directory containing SSL certificates, auto-locates 'ssl_crt.pem/ssl_crt.pwd/ssl_key.pem/ssl_dh.pem'.")

		("ssl_certificate", po::value<std::string>(&ssl_certificate)->value_name("path"), "Path to SSL certificate file.")
		("ssl_certificate_key", po::value<std::string>(&ssl_certificate_key)->value_name("path"), "Path to SSL certificate secret key file.")
		("ssl_certificate_passwd", po::value<std::string>(&ssl_certificate_passwd)->value_name("path/string"), "SSL certificate key passphrase.")
		("ssl_dhparam", po::value<std::string>(&ssl_dhparam)->value_name("path"), "Specifies a file with DH parameters for DHE ciphers.")
		("ssl_sni", po::value<std::string>(&ssl_sni)->value_name("sni"), "Specifies SNI for multiple SSL certificates on one IP.")

		("ssl_ciphers", po::value<std::string>(&ssl_ciphers)->value_name("ssl_ciphers"), "Specify enabled SSL ciphers")
		("ssl_prefer_server_ciphers", po::value<bool>(&ssl_prefer_server_ciphers)->default_value(false, "false")->value_name(""), "Prefer server ciphers over client ciphers for SSLv3 and TLS protocols.")

		("http_doc", po::value<std::string>(&doc_directory)->value_name("doc"), "Specify document root directory for HTTP server.")
		("logs_path", po::value<std::string>(&log_directory)->value_name(""), "Specify directory for log files.")
		("disable_logs", po::value<bool>(&disable_logs)->value_name(""), "Disable logging.")
		("disable_http", po::value<bool>(&disable_http)->value_name("")->default_value(false), "Disable HTTP protocol.")
		("disable_socks", po::value<bool>(&disable_socks)->value_name("")->default_value(false), "Disable SOCKS proxy protocol.")
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
		version_info();
		std::cout << desc;
		return EXIT_SUCCESS;
	}

	if (vm.count("config"))
	{
		if (!fs::exists(config))
		{
			std::cerr << "No such config file: " << config << std::endl;
			return EXIT_FAILURE;
		}

		auto cfg = po::parse_config_file(config.c_str(), desc, false);
		po::store(cfg, vm);
		po::notify(vm);

		if (disable_logs)
			util::toggle_write_logging(true);
	}

	init_logging(log_directory);

	print_args(argc, argv, vm);

	LOG_DBG << "Start server: " << server_listen;

	net::io_context ioc(1);
	server_ptr server;

	net::co_spawn(ioc,
		start_proxy_server(ioc, server),
		net::detached);

	ioc.run();

	return EXIT_SUCCESS;
}
