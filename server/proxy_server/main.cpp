//
// main.cpp
// ~~~~~~~~
//
// Copyright (c) 2023 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/proxy_server.hpp"
#include "proxy/socks_client.hpp"
#include "proxy/logging.hpp"

#include "proxy/use_awaitable.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

#include <boost/algorithm/string/join.hpp>
#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <memory>

#ifdef __linux__
#  include <sys/resource.h>

# ifndef HAVE_UNAME
#  define HAVE_UNAME
# endif

#elif _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <fcntl.h>
#  include <io.h>
#  include <windows.h>

#endif // _WIN32

#ifdef HAVE_UNAME
#  include <sys/utsname.h>
#endif

#include "main.hpp"

namespace net = boost::asio;
using net::ip::tcp;
using namespace proxy;

using server_ptr = std::shared_ptr<proxy_server>;


inline int platform_init()
{
#if defined(WIN32) || defined(_WIN32)
	/* Disable the "application crashed" popup. */
	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
		SEM_NOOPENFILEERRORBOX);

#if defined(DEBUG) ||defined(_DEBUG)
	//	_CrtDumpMemoryLeaks();
	// 	int flags = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
	// 	flags |= _CRTDBG_LEAK_CHECK_DF;
	// 	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
	// 	_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDOUT);
	// 	_CrtSetDbgFlag(flags);
#endif

#if !defined(__MINGW32__)
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
#endif

	_setmode(0, _O_BINARY);
	_setmode(1, _O_BINARY);
	_setmode(2, _O_BINARY);

	/* Disable stdio output buffering. */
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

	/* Enable minidump when application crashed. */
#elif defined(__linux__)
	rlimit of = { 50000, 100000 };
	setrlimit(RLIMIT_NOFILE, &of);

	struct rlimit core_limit;
	core_limit.rlim_cur = RLIM_INFINITY;
	core_limit.rlim_max = RLIM_INFINITY;
	setrlimit(RLIMIT_CORE, &core_limit);

	/* Set the stack size programmatically with setrlimit */
	rlimit rl;
	int result = getrlimit(RLIMIT_STACK, &rl);
	if (result == 0)
	{
		const rlim_t stack_size = 100 * 1024 * 1024;
		if (rl.rlim_cur < stack_size)
		{
			rl.rlim_cur = stack_size;
			setrlimit(RLIMIT_STACK, &rl);
		}
	}
#endif

	std::ios::sync_with_stdio(false);

	return 0;
}

//////////////////////////////////////////////////////////////////////////

std::string socks_userid;
std::string socks_passwd;
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
std::string socks_listen;
std::string doc_directory;
std::string log_directory;
bool disable_http = false;
bool disable_logs;
bool reuse_port = false;

//////////////////////////////////////////////////////////////////////////

net::awaitable<void>
start_proxy_server(net::io_context& ioc, server_ptr& server)
{
	std::string host, port;
	bool v6only = false;

	if (!parse_endpoint_string(socks_listen, host, port, v6only))
	{
		std::cerr << "Parse endpoint fail: " << socks_listen << std::endl;
		co_return;
	}

	tcp::endpoint listen(
		net::ip::address::from_string(host),
			(unsigned short)atoi(port.c_str()));

	proxy_server_option opt;

	opt.usrdid_ = socks_userid;
	opt.passwd_ = socks_passwd;

	opt.proxy_pass_ = proxy_pass;
	opt.proxy_pass_use_ssl_ = proxy_pass_ssl;

	opt.ssl_cert_path_ = ssl_certificate_dir;
	opt.ssl_ciphers_ = ssl_ciphers;
	opt.ssl_certificate_ = ssl_certificate;
	opt.ssl_certificate_key_ = ssl_certificate_key;
	opt.ssl_certificate_passwd_ = ssl_certificate_passwd;
	opt.ssl_dhparam_ = ssl_dhparam;
	opt.ssl_sni_ = ssl_sni;

	opt.disable_noraml_http_ = disable_http;
	opt.reuse_port_ = reuse_port;

	opt.doc_directory_ = doc_directory;

	server = proxy_server::make(
		ioc.get_executor(), listen, opt);
	server->start();

	co_return;
}

//////////////////////////////////////////////////////////////////////////

inline std::string version_info()
{
	std::string os_name;

#ifdef _WIN32
#ifdef _WIN64
	os_name = "Windows (64bit)";
#else
	os_name = "Windows (32bit)";
#endif // _WIN64

#elif defined(HAVE_UNAME)
	utsname un;
	uname(&un);
	os_name = un.sysname;
	os_name += " ";
	os_name += un.release;

	// extract_linux_version from un.release;
	int ma_ver, mi_ver, patch_ver;
	sscanf(un.release, "%d.%d.%d", &ma_ver, &mi_ver, &patch_ver);

	if (std::string(un.sysname) == "Linux" && ma_ver < 4)
	{
		std::cerr << "WARNING: kernel too old, "
			<< "please upgrade your system!" << std::endl;
	}

#elif defined(__APPLE__)
	os_name = "Drawin";
#else
	os_name = "unknow";
#endif // _WIN32

	std::ostringstream oss;
	oss << "Built on " << __DATE__
		<< " " << __TIME__
		<< " runs on " << os_name
		<< ", " << BOOST_COMPILER
		<< ", boost " << BOOST_LIB_VERSION;
	std::cerr << oss.str() << "\n";

	return oss.str();
}

void print_args(int argc, char** argv,
	const po::variables_map& vm)
{
	LOG_INFO << "Current directory: "
		<< std::filesystem::current_path().string();

	if (!vm.count("config"))
	{
		std::vector<std::string> print_args;
		print_args.assign(argv, argv + argc);
		LOG_INFO << "Run: "
			<< boost::algorithm::join(print_args, " ");

		return;
	}

	for (const auto& cfg : vm)
	{
		if (cfg.second.empty() || cfg.first == "config")
			continue;

		auto& var = cfg.second.value();
		try {
			const auto& s = boost::any_cast<std::string>(var);
			LOG_INFO << cfg.first
				<< " = "
				<< s;
			continue;
		}
		catch (const std::exception&) {}

		try {
			const auto& v = boost::any_cast<bool>(var);
			LOG_INFO << cfg.first
				<< " = "
				<< v;
			continue;
		}
		catch (const std::exception&) {}

		try {
			const auto& v = boost::any_cast<int>(var);
			LOG_INFO << cfg.first
				<< " = "
				<< v;
			continue;
		}
		catch (const std::exception&) {}
	}
}


int main(int argc, char** argv)
{
	platform_init();

	std::string config;

	po::options_description desc("Options");
	desc.add_options()
		("help,h", "Help message.")
		("config", po::value<std::string>(&config)->value_name("config.conf"), "Load config options from file.")

		("socks_server", po::value<std::string>(&socks_listen)->default_value("[::0]:1080")->value_name("ip:port"), "For socks4/5 server listen.")

		("reuse_port", po::value<bool>(&reuse_port)->default_value(false), "TCP reuse port option(SO_REUSEPORT since Linux 3.9).")

		("socks_userid", po::value<std::string>(&socks_userid)->default_value("jack")->value_name("userid"), "Socks4/5 auth user id.")
		("socks_passwd", po::value<std::string>(&socks_passwd)->default_value("1111")->value_name("passwd"), "Socks4/5 auth password.")

		("proxy_pass", po::value<std::string>(&proxy_pass)->default_value("")->value_name(""), "Next proxy pass. (e.g: socks5://user:passwd@ip:port)")
		("proxy_pass_ssl", po::value<bool>(&proxy_pass_ssl)->default_value(false, "false")->value_name(""), "Next proxy pass with ssl.")

		("ssl_certificate_dir", po::value<std::string>(&ssl_certificate_dir)->value_name("path"), "SSL certificate dir, auto find 'ssl_crt.pem/ssl_crt.pwd/ssl_key.pem/ssl_dh.pem'.")

		("ssl_certificate", po::value<std::string>(&ssl_certificate)->value_name("path"), "SSL certificate file.")
		("ssl_certificate_key", po::value<std::string>(&ssl_certificate_key)->value_name("path"), "SSL certificate secret key.")
		("ssl_certificate_passwd", po::value<std::string>(&ssl_certificate_passwd)->value_name("path/string"), "SSL certificate key passphrases.")
		("ssl_dhparam", po::value<std::string>(&ssl_dhparam)->value_name("path"), "Specifies a file with DH parameters for DHE ciphers.")
		("ssl_sni", po::value<std::string>(&ssl_sni)->value_name("sni"), "Specifies SNI for multiple SSL certificates on one IP.")

		("ssl_ciphers", po::value<std::string>(&ssl_ciphers)->value_name("ssl_ciphers"), "Specifies the enabled ciphers.")
		("ssl_prefer_server_ciphers", po::value<bool>(&ssl_prefer_server_ciphers)->default_value(false, "false")->value_name(""), "Specifies that server ciphers should be preferred over client ciphers when using the SSLv3 and TLS protocols.")

		("http_doc", po::value<std::string>(&doc_directory)->value_name("doc"), "Http server doc root.")
		("logs_path", po::value<std::string>(&log_directory)->value_name(""), "Logs dirctory.")
		("disable_logs", po::value<bool>(&disable_logs)->value_name(""), "Disable logs.")
		("disable_http", po::value<bool>(&disable_http)->value_name("")->default_value(false), "Disable http protocol.")
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
		std::cout << version_info() << desc;
		return EXIT_SUCCESS;
	}

	init_logging(log_directory);

	if (vm.count("config"))
	{
		if (!std::filesystem::exists(config))
		{
			std::cerr << "No such config file: " << config << std::endl;
			return EXIT_FAILURE;
		}

		LOG_DBG << "Load config file: " << config;
		auto cfg = po::parse_config_file(config.c_str(), desc, false);
		po::store(cfg, vm);
		po::notify(vm);

		if (disable_logs)
			util::toggle_write_logging(true);
	}

	print_args(argc, argv, vm);

	LOG_DBG << "Start socks server: " << socks_listen;

	net::io_context ioc(1);
	server_ptr server;

	net::co_spawn(ioc, start_proxy_server(ioc, server), net::detached);

	ioc.run();

	return EXIT_SUCCESS;
}
