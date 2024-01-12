//
// main.cpp
// ~~~~~~~~
//
// Copyright (c) 2023 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//


#include <limits>

#include <boost/asio/io_context.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/signal_set.hpp>

#include <boost/algorithm/string/join.hpp>
#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <filesystem>
namespace fs = std::filesystem;

#include "proxy/proxy_server.hpp"
#include "proxy/socks_client.hpp"
#include "proxy/logging.hpp"

#include "proxy/use_awaitable.hpp"
#include "proxy/ipip.hpp"

#include "main.hpp"

namespace net = boost::asio;
using namespace proxy;

using server_ptr = std::shared_ptr<proxy_server>;

#ifdef _MSC_VER
# pragma warning(disable: 4244)
#endif


//////////////////////////////////////////////////////////////////////////


std::vector<std::string> auth_users;
std::vector<std::string> deny_region;
std::vector<std::string> allow_region;

std::string ipip_db;
std::string doc_dir;
std::string log_dir;
std::string local_ip;
std::string proxy_pass;
std::string server_listen;
std::string ssl_cert;
std::string ssl_cert_dir;
std::string ssl_cert_key;
std::string ssl_cert_pwd;
std::string ssl_ciphers;
std::string ssl_dhparam;
std::string ssl_sni;

bool transparent = false;
bool autoindex = false;
bool disable_http = false;
bool disable_insecure = true;
bool disable_logs;
bool disable_socks = false;
bool happyeyeballs = true;
bool connect_v6only = false;
bool connect_v4only = false;
bool proxy_pass_ssl = false;
bool reuse_port = false;
bool scramble = false;
bool ssl_prefer_server_ciphers = false;

int64_t linux_so_mark;
uint16_t noise_length;


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

	opt.ssl_cert_path_ = ssl_cert_dir;
	opt.ssl_ciphers_ = ssl_ciphers;
	opt.ssl_certificate_ = ssl_cert;
	opt.ssl_certificate_key_ = ssl_cert_key;
	opt.ssl_certificate_passwd_ = ssl_cert_pwd;
	opt.ssl_dhparam_ = ssl_dhparam;
	opt.ssl_sni_ = ssl_sni;

	opt.disable_http_ = disable_http;
	opt.disable_socks_ = disable_socks;
	opt.disable_insecure_ = disable_insecure;
	opt.scramble_ = scramble;
	opt.noise_length_ = noise_length;
	opt.local_ip_ = local_ip;

	opt.ipip_db_ = ipip_db;
	opt.deny_regions_ = deny_region;
	opt.allow_regions_ = allow_region;

	opt.reuse_port_ = reuse_port;
	opt.happyeyeballs_ = happyeyeballs;
	opt.connect_v4_only_ = connect_v4only;
	opt.connect_v6_only_ = connect_v6only;
	opt.transparent_ = transparent;
	if (linux_so_mark > 0 && linux_so_mark <= std::numeric_limits<uint32_t>::max())
		opt.so_mark_.emplace(linux_so_mark);

	opt.doc_directory_ = doc_dir;
	opt.autoindex_ = autoindex;

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
	XLOG_INFO << "Current directory: "
		<< fs::current_path().string();

	if (!vm.count("config"))
	{
		std::vector<std::string> args(argv, argv + argc);
		XLOG_INFO << "Run: " << boost::algorithm::join(args, " ");
		return;
	}

	for (const auto& [key, value] : vm)
	{
		if (value.empty() || key == "config")
			continue;

		if (auto s = try_as_string(value.value()))
		{
			XLOG_INFO << key << " = " << *s;
			continue;
		}

		if (auto b = try_as_bool(value.value()))
		{
			XLOG_INFO << key << " = " << *b;
			continue;
		}

		if (auto i = try_as_int(value.value()))
		{
			XLOG_INFO << key << " = " << *i;
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

		("v6only", po::value<bool>(&connect_v6only)->default_value(false), "Enable IPv6 only mode for TCP connections.")
		("v4only", po::value<bool>(&connect_v4only)->default_value(false), "Enable IPv4 only mode for TCP connections.")

		("local_ip", po::value<std::string>(&local_ip), "Specify local IP for client TCP connection to server.")

		("transparent", po::value<bool>(&transparent)->default_value(false), "Enable transparent proxy mode(only linux).")
		("so_mark", po::value<int64_t>(&linux_so_mark)->default_value(-1), "Set SO_MARK for linux transparent proxy mode.")

		("auth_users", po::value<std::vector<std::string>>(&auth_users)->multitoken()->default_value(std::vector<std::string>{"jack:1111"}), "List of authorized users(default user: jack:1111) (e.g: user1:passwd1 user2:passwd2).")

		("allow_region", po::value<std::vector<std::string>>(&allow_region)->multitoken(), "Allow region (e.g: 北京|河南|武汉).")
		("deny_region", po::value<std::vector<std::string>>(&deny_region)->multitoken(), "Deny region (e.g: 广东|上海|山东).")

		("proxy_pass", po::value<std::string>(&proxy_pass)->default_value("")->value_name(""), "Specify next proxy pass (e.g: socks5://user:passwd@ip:port).")
		("proxy_pass_ssl", po::value<bool>(&proxy_pass_ssl)->default_value(false, "false")->value_name(""), "Enable SSL for the next proxy pass.")

		("ssl_certificate_dir", po::value<std::string>(&ssl_cert_dir)->value_name("path"), "Directory containing SSL certificates, auto-locates 'ssl_crt.pem/ssl_crt.pwd/ssl_key.pem/ssl_dh.pem'.")

		("ssl_certificate", po::value<std::string>(&ssl_cert)->value_name("path"), "Path to SSL certificate file.")
		("ssl_certificate_key", po::value<std::string>(&ssl_cert_key)->value_name("path"), "Path to SSL certificate secret key file.")
		("ssl_certificate_passwd", po::value<std::string>(&ssl_cert_pwd)->value_name("path/string"), "SSL certificate key passphrase.")
		("ssl_dhparam", po::value<std::string>(&ssl_dhparam)->value_name("path"), "Specifies a file with DH parameters for DHE ciphers.")
		("ssl_sni", po::value<std::string>(&ssl_sni)->value_name("sni"), "Specifies SNI for multiple SSL certificates on one IP.")

		("ssl_ciphers", po::value<std::string>(&ssl_ciphers)->value_name("ssl_ciphers"), "Specify enabled SSL ciphers")
		("ssl_prefer_server_ciphers", po::value<bool>(&ssl_prefer_server_ciphers)->default_value(false, "false")->value_name(""), "Prefer server ciphers over client ciphers for SSLv3 and TLS protocols.")

		("ipip_db", po::value<std::string>(&ipip_db)->value_name("")->default_value("17monipdb.datx"), "Specify ipip database filename.")
		("http_doc", po::value<std::string>(&doc_dir)->value_name("doc"), "Specify document root directory for HTTP server.")
		("autoindex", po::value<bool>(&autoindex)->default_value(false), "Enable directory listing.")
		("logs_path", po::value<std::string>(&log_dir)->value_name(""), "Specify directory for log files.")
		("disable_logs", po::value<bool>(&disable_logs)->value_name(""), "Disable logging.")
		("disable_http", po::value<bool>(&disable_http)->value_name("")->default_value(false), "Disable HTTP protocol.")
		("disable_socks", po::value<bool>(&disable_socks)->value_name("")->default_value(false), "Disable SOCKS proxy protocol.")
		("disable_insecure", po::value<bool>(&disable_insecure)->value_name("")->default_value(false), "Disable insecure protocol.")
		("scramble", po::value<bool>(&scramble)->value_name("")->default_value(false), "Noise-based data security.")
		("noise_length", po::value<uint16_t>(&noise_length)->value_name("length")->default_value(0x0fff), "Length of the noise data.")
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

	init_logging(log_dir);

	print_args(argc, argv, vm);

	XLOG_DBG << "Start server: " << server_listen;

	net::io_context ioc(1);
	net::signal_set terminator_signal(ioc);
	server_ptr server;

	terminator_signal.add(SIGINT);
	terminator_signal.add(SIGTERM);
#ifdef __linux__
	signal(SIGPIPE, SIG_IGN);
#endif
#if defined(SIGQUIT)
	terminator_signal.add(SIGQUIT);
#endif // defined(SIGQUIT)

	terminator_signal.async_wait(
		[&](const boost::system::error_code&, int sig) mutable
			{
				terminator_signal.remove(sig);
				ioc.stop();
			});

	net::co_spawn(ioc,
		start_proxy_server(ioc, server),
		net::detached);

	ioc.run();

	return EXIT_SUCCESS;
}
