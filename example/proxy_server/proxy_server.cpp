//
// proxy_server.cpp
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2022 Jack (jack dot wgm at gmail dot com)
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

namespace net = boost::asio;
using net::ip::tcp;
using namespace proxy;

using server_ptr = std::shared_ptr<proxy_server>;

//////////////////////////////////////////////////////////////////////////

inline bool is_space(const char c)
{
	if (c == ' ' ||
		c == '\f' ||
		c == '\n' ||
		c == '\r' ||
		c == '\t' ||
		c == '\v')
		return true;
	return false;
}

//////////////////////////////////////////////////////////////////////////

inline std::string_view string_trim(std::string_view sv)
{
	const char* b = sv.data();
	const char* e = b + sv.size();

	for (; b != e; b++)
	{
		if (!is_space(*b))
			break;
	}

	for (; e != b; )
	{
		if (!is_space(*(--e)))
		{
			++e;
			break;
		}
	}

	return std::string_view(b, e - b);
}

//////////////////////////////////////////////////////////////////////////

inline bool parse_endpoint_string(std::string_view str,
	std::string& host, std::string& port, bool& ipv6only)
{
	ipv6only = false;

	auto address_string = string_trim(str);
	auto it = address_string.begin();

	bool is_ipv6_address = *it == '[';
	if (is_ipv6_address)
	{
		auto host_end = std::find(it, address_string.end(), ']');
		if (host_end == address_string.end())
			return false;

		it++;
		for (auto first = it; first != host_end; first++)
			host.push_back(*first);

		std::advance(it, host_end - it);
		it++;
	}
	else
	{
		auto host_end = std::find(it, address_string.end(), ':');
		if (host_end == address_string.end())
			return false;

		for (auto first = it; first != host_end; first++)
			host.push_back(*first);

		// Skip host.
		std::advance(it, host_end - it);
	}

	if (*it != ':')
		return false;

	it++;
	for (; it != address_string.end(); it++)
	{
		if (*it >= '0' && *it <= '9')
		{
			port.push_back(*it);
			continue;
		}

		break;
	}

	if (it != address_string.end())
	{
#ifdef __cpp_lib_to_address
		auto opt = std::string_view(
			std::to_address(it), address_string.end() - it);
#else
		auto opt = std::string(it, address_string.end());
#endif
		if (opt == "ipv6only" || opt == "-ipv6only")
			ipv6only = true;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////

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
std::string socks_next_proxy;
bool socks_next_proxy_ssl = false;
std::string ssl_certificate_dir;
std::string socks_listen;
std::string doc_directory;
std::string log_directory;

//////////////////////////////////////////////////////////////////////////

net::awaitable<void> start_proxy_server(server_ptr& server)
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

	opt.next_proxy_ = socks_next_proxy;
	opt.next_proxy_use_ssl_ = socks_next_proxy_ssl;
	opt.ssl_cert_path_ = ssl_certificate_dir;
	opt.doc_directory_ = doc_directory;

	auto executor = co_await net::this_coro::executor;
	server = proxy_server::make(
		executor, listen, opt);
	server->start();

	co_return;
}

//////////////////////////////////////////////////////////////////////////

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

		("socks_userid", po::value<std::string>(&socks_userid)->default_value("jack")->value_name("userid"), "Socks4/5 auth user id.")
		("socks_passwd", po::value<std::string>(&socks_passwd)->default_value("1111")->value_name("passwd"), "Socks4/5 auth password.")

		("socks_next_proxy", po::value<std::string>(&socks_next_proxy)->default_value("")->value_name(""), "Next socks4/5 proxy. (e.g: socks5://user:passwd@ip:port)")
		("socks_next_proxy_ssl", po::value<bool>(&socks_next_proxy_ssl)->default_value(false, "false")->value_name(""), "Next socks4/5 proxy with ssl.")

		("ssl_certificate_dir", po::value<std::string>(&ssl_certificate_dir)->value_name("path"), "SSL certificate dir.")

		("http_doc", po::value<std::string>(&doc_directory)->value_name("doc"), "Http server doc root.")
		("logs_path", po::value<std::string>(&log_directory)->value_name(""), "Logs dirctory.")
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

	init_logging(log_directory);

	if (vm.count("config"))
	{
		if (!std::filesystem::exists(config))
		{
			LOG_ERR << "No such config file: " << config;
			return EXIT_FAILURE;
		}

		LOG_DBG << "Load config file: " << config;
		auto cfg = po::parse_config_file(config.c_str(), desc, false);
		po::store(cfg, vm);
		po::notify(vm);
	}

	print_args(argc, argv, vm);

	LOG_DBG << "Start socks server: " << socks_listen;

	net::io_context ioc(1);
	server_ptr server;

	net::co_spawn(ioc, start_proxy_server(server), net::detached);

	ioc.run();

	return EXIT_SUCCESS;
}
