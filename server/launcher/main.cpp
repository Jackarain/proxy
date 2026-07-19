//
// main.cpp
// ~~~~~~~~
//
// Copyright (c) 2023 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifdef _MSC_VER
# pragma warning(push)
# pragma warning(disable: 4005 4267 4244)
#endif // _MSC_VER

#ifdef USE_SNMALLOC
# ifdef NDEBUG
#  define SNMALLOC_STATIC_LIBRARY_PREFIX sn_
#  include "src/snmalloc/override/malloc.cc"
#  include "src/snmalloc/override/new.cc"
# endif
#endif // USE_SNMALLOC


#include "proxy/logging.hpp"
#include "proxy/use_awaitable.hpp"

#include "main.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/local/stream_protocol.hpp>

#include <boost/nowide/args.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;


#include <limits>

#ifdef _MSC_VER
# pragma warning(pop)
#endif

namespace net = boost::asio;

//////////////////////////////////////////////////////////////////////////

std::string config;
std::string asio_config;


net::awaitable<void> start_launcher_server(net::io_context& ioc)
{
	co_return;
}


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
	[[maybe_unused]]boost::nowide::args a(argc, argv);
	platform_init();

	std::string config;

#ifdef NDEBUG
	const bool default_logs = true;
#else
	const bool default_logs = false;
#endif

	po::options_description desc("Options");
	desc.add_options()
		("help,h", "Show this help message and exit.")
		("config", po::value<std::string>(&config)->value_name("config.conf"), "Load configuration options from the specified file.")
		("asio_config", po::value<std::string>(&asio_config)->value_name("enable asio config env")->default_value("ASIO"), "Environment variable name for configuring Boost.Asio (default: ASIO).")
	;

	// 解析命令行.
	po::variables_map vm;

	try {
		po::store(
			po::command_line_parser(argc, argv)
			.options(desc)
			.style(po::command_line_style::unix_style
				| po::command_line_style::allow_long_disguise)
			.run()
			, vm);
		po::notify(vm);
	}
	catch (const po::error& e)
	{
		std::cerr << "Error parsing command line: " << e.what() << "\n";
		return EXIT_FAILURE;
	}

	// 帮助输出.
	if (vm.count("help") || argc == 1)
	{
		version_info();
		std::cout << desc
				  << "\n"
				  << R"(Email bug reports, questions, discussions to <jack.wgm@gmail.com>
and/or open issues at https://github.com/Jackarain/proxy)"
				  << "\n";
		return EXIT_SUCCESS;
	}

	if (vm.count("config"))
	{
		boost::system::error_code ignore_ec;
		if (!fs::exists(config, ignore_ec))
		{
			std::cerr << "No such config file: " << config << std::endl;
			return EXIT_FAILURE;
		}

		try {
			auto cfg = po::parse_config_file(config.c_str(), desc, false);
			po::store(cfg, vm);
			po::notify(vm);
		}
		catch (const po::error& e)
		{
			std::cerr << "Error parsing config file: " << e.what() << "\n";
			return EXIT_FAILURE;
		}
	}

	print_args(argc, argv, vm);

	auto cfg = net::config_from_env(asio_config);
	net::io_context ioc(cfg);
	net::signal_set terminator_signal(ioc);

#ifdef __linux__
	signal(SIGPIPE, SIG_IGN);
#endif

	net::co_spawn(ioc, start_launcher_server(ioc), net::detached);
	ioc.run();

	return EXIT_SUCCESS;
}
