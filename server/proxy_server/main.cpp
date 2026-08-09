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

#include "proxy/proxy.hpp"
#include "proxy/socks_client.hpp"
#include "proxy/logging.hpp"

#include "proxy/use_awaitable.hpp"

#include "main.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/local/stream_protocol.hpp>

#include <boost/nowide/args.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <cmath>
#include <fstream>
#include <limits>

#ifdef _MSC_VER
# pragma warning(pop)
#endif

namespace net = boost::asio;
using namespace proxy;

using server_ptr = std::shared_ptr<proxy_server>;


//////////////////////////////////////////////////////////////////////////

std::vector<std::string> server_listens;
std::vector<std::string> auth_users;
std::vector<std::string> users_rate_limit;
std::vector<std::string> users_quota;
std::vector<std::string> deny_region;
std::vector<std::string> allow_region;

std::string ipip_db_name;
std::string doc_dir;
std::string log_dir;
std::string stdio_target;
std::string local_ip;
std::string proxy_pass;
std::string ssl_cert_dir;
std::string ssl_cacert_dir;
std::string ssl_ciphers;
std::string ssl_dhparam;
std::string proxy_ssl_name;
std::string dns_upstream;

std::string pam_auth;

bool transparent = false;
bool autoindex = false;
bool htpasswd = false;

bool disable_http = false;
bool disable_insecure = true;
bool disable_logs;
bool disable_socks = false;
bool disable_udp = false;
bool disable_check_cert = false;

bool happyeyeballs = true;
bool connect_v6only = false;
bool connect_v4only = false;
bool proxy_pass_ssl = false;
bool reuse_port = false;
bool scramble = false;
bool ssl_prefer_server_ciphers = false;

int64_t linux_so_mark;
int64_t noise_length;
int tcp_timeout;
int udp_timeout;
int rate_limit;

std::string asio_config;

// launcher 控制通道相关.
std::string launcher_url;
std::string pid_file;
// launcher 生成参数的选项（本版本接受但不实施）.
int dns_udp_port = 0;
int dns_cache_size = 0;
int dns_cache_ttl = 0;
bool dns_no_ipv6 = false;
bool http2 = false;

//////////////////////////////////////////////////////////////////////////

net::awaitable<void>
start_proxy_server(net::io_context& ioc, server_ptr& server)
{
	proxy_server_option opt;
	auto& listens = opt.listens_;
	auto& uds_listens = opt.uds_listens_;
	boost::system::error_code ec;

	for (const auto& listen : server_listens)
	{
		std::string host, port;
		bool v6only = false;

		if (listen.starts_with("unix://"))
		{
			auto uds_path = listen.substr(7);
			uds_listens.emplace_back(uds_path);
			fs::remove(uds_path, ec);

			continue;
		}

		if (!parse_endpoint_string(listen, host, port, v6only))
		{
			XLOG_ERR << "Parse listen endpoint fail: " << listen;
			co_return;
		}

		listens.emplace_back(
			tcp::endpoint{
				net::ip::make_address(host, ec),
				(unsigned short)atoi(port.c_str())},
			v6only
		);

		if (ec)
		{
			XLOG_ERR << "Parse make endpoint fail: " << listen
				<< ", error: " << ec.message();
			co_return;
		}
	}

	for (const auto& auth_user : auth_users)
	{
		// 解析 user:password:addr:proxy_pass 格式.
		std::vector<std::string> parts;
		std::string token;
		std::stringstream ss(auth_user);

		for (int i = 0; i < 3 && std::getline(ss, token, ':'); i++)
			parts.push_back(token);

		if (ss)
		{
			std::string remaining;
			std::getline(ss, remaining);
			if (!remaining.empty())
				parts.push_back(remaining);
		}

		std::string user, password, addr, proxy_url;

		if (parts.size() > 0) user = parts[0];
		if (parts.size() > 1) password = parts[1];
		if (parts.size() > 2) addr = parts[2];
		if (parts.size() > 3) proxy_url = parts[3];

		if (user.empty() && password.empty() && addr.empty() && proxy_url.empty())
			continue;

		std::optional<urls::url> proxy_url_result;
		if (!proxy_url.empty())
		{
			auto result = urls::parse_uri(proxy_url);
			if (result.has_value())
			{
				proxy_url_result = result.value();
			}
			else
			{
				XLOG_ERR << "Parse user: " << user << ", proxy url: " << result.error().message();
				co_return;
			}
		}

		opt.auth_users_.emplace_back(user, password, addr, proxy_url_result);
	}

	for (const auto& user : users_rate_limit)
	{
		if (user.empty())
			continue;

		auto pos = user.find(':');
		if (pos == std::string::npos)
			continue;

		auto name = user.substr(0, pos);
		auto rate = std::atoi(user.substr(pos + 1).c_str());

		opt.users_rate_limit_.insert_or_assign(name, rate);
	}

	for (const auto& entry : users_quota)
	{
		if (entry.empty())
			continue;

		auto pos = entry.find(':');
		if (pos == std::string::npos)
			continue;

		auto name = entry.substr(0, pos);
		std::int64_t quota = 0;
		try {
			quota = std::stoll(entry.substr(pos + 1));
		} catch (...) {
			continue;
		}
		opt.users_quota_.insert_or_assign(name, quota);
	}

	if (!proxy_pass.empty())
	{
		auto result = urls::parse_uri(proxy_pass);
		if (result.has_value())
		{
			opt.proxy_pass_ = result.value();
		}
		else
		{
			XLOG_ERR << "Parse proxy url: " << result.error().message();
			co_return;
		}
	}

	opt.stdio_target_ = stdio_target;
	if (!stdio_target.empty())
	{
		if (!opt.proxy_pass_)
		{
			XLOG_ERR << "stdio proxy requires a proxy_pass";
			co_return;
		}
	}

#ifdef USE_PAM_AUTH
	opt.pam_auth_ = pam_auth;
#endif
	opt.proxy_pass_use_ssl_ = proxy_pass_ssl;

	opt.ssl_cert_path_ = ssl_cert_dir;
	opt.ssl_cacert_path_ = ssl_cacert_dir;

	opt.ssl_ciphers_ = ssl_ciphers;
	opt.proxy_ssl_name_ = proxy_ssl_name;

	opt.disable_http_ = disable_http;
	opt.disable_socks_ = disable_socks;
	opt.disable_udp_ = disable_udp;
	opt.disable_insecure_ = disable_insecure;
	opt.disable_check_cert_ = disable_check_cert;
	opt.scramble_ = scramble;
	opt.noise_length_ = noise_length;
	opt.local_ip_ = local_ip;
	opt.tcp_timeout_ = tcp_timeout;
	opt.udp_timeout_ = udp_timeout == -1 ? std::numeric_limits<int>::max() : udp_timeout;
	opt.tcp_rate_limit_ = rate_limit;

	opt.ipip_db_ = ipip_db_name;
	if (!deny_region.empty())
	{
		std::vector<std::string> regions;

		for (const auto& region : deny_region)
		{
			auto ret = strutil::split(region, "|");
			regions.insert(regions.end(), ret.begin(), ret.end());
		}
		deny_region = regions;
	}
	opt.deny_regions_.insert(deny_region.begin(), deny_region.end());
	if (!allow_region.empty())
	{
		std::vector<std::string> regions;

		for (const auto& region : allow_region)
		{
			auto ret = strutil::split(region, "|");
			regions.insert(regions.end(), ret.begin(), ret.end());
		}
		allow_region = regions;
	}
	opt.allow_regions_.insert(allow_region.begin(), allow_region.end());

	opt.reuse_port_ = reuse_port;
	opt.happyeyeballs_ = happyeyeballs;
	opt.connect_v4_only_ = connect_v4only;
	opt.connect_v6_only_ = connect_v6only;
	opt.transparent_ = transparent;
	if (opt.transparent_)
	{
		if (proxy_pass.empty())
		{
			XLOG_ERR << "transparent proxy requires a proxy_pass";
			co_return;
		}
	}
	if (linux_so_mark > 0 && linux_so_mark <= std::numeric_limits<uint32_t>::max())
		opt.so_mark_.emplace(linux_so_mark);

	opt.doc_directory_ = doc_dir;
	opt.autoindex_ = autoindex;
	opt.htpasswd_ = htpasswd;
	if (!dns_upstream.empty())
		opt.dns_upstream_ = dns_upstream;

	// launcher 控制通道：URL 经 opt 传入, proxy_server 在 start() 时自动启动.
	// 收到 launcher 的 shutdown 请求时, proxy_server 内部关闭所有 session/连接/
	// accept/定时器, 使进程自然退出, 无需外部显式启停.
	opt.launcher_url_ = launcher_url;
	if (!launcher_url.empty())
	{
		// 检查 launcher url 是否为 websocket 协议（ws:// 或 wss://），否则无法与 launcher 建立控制通道.
		if (launcher_url.rfind("ws://", 0) != 0 && launcher_url.rfind("wss://", 0) != 0)
		{
			XLOG_ERR << "invalid --launcher url: " << launcher_url;
			co_return;
		}

		// 关闭控制台日志颜色输出，避免 launcher 控制台日志被 ANSI 颜色码污染.
		xlogger::toggle_console_logging_color(false);
	}

	server = proxy_server::make(ioc.get_executor(), opt);
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
	[[maybe_unused]]boost::nowide::args a(argc, argv);
	platform_init();

	std::string config;

	// 默认开启控制台日志
	// 的行为一致）。launcher 仅在用户显式关闭日志时才传 --disable_logs true，
	// 因此这里默认值必须为 false，否则 launcher 管理下控制台日志会被静默关闭。
	const bool default_logs = false;

	po::options_description desc("Options");
	desc.add_options()
		("help,h", "Show this help message and exit.")
		("config", po::value<std::string>(&config)->value_name("config.conf"), "Load configuration options from the specified file.")

		("server_listen", po::value<std::vector<std::string>>(&server_listens)->default_value({ "[::0]:1080" })->value_name("ip:port [ip:port ...]"), "Specify the server listening address and port (repeatable).")
		("stdio", po::value<std::string>(&stdio_target), "Specify the destination host:port for stdio proxy mode.")

		("reuse_port", po::value<bool>(&reuse_port)->default_value(false, "false"), "Enable the TCP SO_REUSEPORT socket option (available since Linux 3.9).")
		("happyeyeballs", po::value<bool>(&happyeyeballs)->default_value(true, "true"), "Enable the Happy Eyeballs algorithm for outbound TCP connections.")

		("v6only", po::value<bool>(&connect_v6only)->default_value(false, "false"), "Restrict outbound TCP connections to IPv6 only.")
		("v4only", po::value<bool>(&connect_v4only)->default_value(false, "false"), "Restrict outbound TCP connections to IPv4 only.")

		("local_ip", po::value<std::string>(&local_ip), "Specify the local IP address for outbound TCP connections.")

		("transparent", po::value<bool>(&transparent)->default_value(false, "false"), "Enable transparent proxy mode (Linux only).")
		("so_mark", po::value<int64_t>(&linux_so_mark)->default_value(-1), "Set the SO_MARK socket option for outbound connections (Linux only).")

		("tcp_timeout", po::value<int>(&tcp_timeout)->default_value(-1), "Set the idle timeout in seconds for TCP connections (-1 = disable).")
		("udp_timeout", po::value<int>(&udp_timeout)->default_value(60), "Set the idle timeout in seconds for UDP sessions.")
		("rate_limit", po::value<int>(&rate_limit)->default_value(-1), "Set the TCP rate limit in bytes/second per connection (-1 = disable).")

		("pam_auth", po::value<std::string>(&pam_auth)->value_name("pam service"), "Enable PAM authentication with the specified PAM service name.")

		("auth_users", po::value<std::vector<std::string>>(&auth_users)->multitoken()->default_value(std::vector<std::string>{"jack:1111"}), "List of authorized users (default: jack:1111), format: user:password[:addr[:proxy_url]] (repeatable).")
		("users_rate_limit", po::value<std::vector<std::string>>(&users_rate_limit)->multitoken(), "Per-user rate limit in bytes/second, format: user:rate (repeatable).")
		("users_quota", po::value<std::vector<std::string>>(&users_quota)->multitoken(), "Per-user download traffic quota in bytes, format: user:bytes (repeatable).")

		("allow_region", po::value<std::vector<std::string>>(&allow_region)->multitoken(), "Allow connections only from the specified regions/CIDRs (repeatable).")
		("deny_region", po::value<std::vector<std::string>>(&deny_region)->multitoken(), "Deny connections from the specified regions/CIDRs (repeatable).")

		("proxy_pass", po::value<std::string>(&proxy_pass)->default_value("")->value_name(""), "Specify the upstream proxy URL (e.g: socks5://user:passwd@ip:port).")
		("proxy_pass_ssl", po::value<bool>(&proxy_pass_ssl)->default_value(false, "false")->value_name(""), "Enable SSL/TLS for the upstream proxy connection.")

		("ssl_certificate_dir", po::value<std::string>(&ssl_cert_dir)->value_name("path"), "Directory containing SSL/TLS certificates.")
		("ssl_cacert_dir", po::value<std::string>(&ssl_cacert_dir)->value_name("path"), "Directory containing SSL/TLS CA certificates for verification.")

		("ssl_sni", po::value<std::string>(&proxy_ssl_name)->value_name("sni"), "Specify the SNI hostname for multiple SSL certificates on one IP (deprecated, use proxy_ssl_name instead).")
		("proxy_ssl_name", po::value<std::string>(&proxy_ssl_name)->value_name("sni"), "Specify the SNI hostname for multiple SSL certificates on one IP.")

		("ssl_ciphers", po::value<std::string>(&ssl_ciphers)->value_name("ssl_ciphers"), "Specify the enabled SSL/TLS ciphers.")
		("ssl_prefer_server_ciphers", po::value<bool>(&ssl_prefer_server_ciphers)->default_value(false, "false")->value_name(""), "Prefer server ciphers over client ciphers for SSLv3 and TLS protocols.")

		("ipip_db", po::value<std::string>(&ipip_db_name)->value_name("")->default_value("17monipdb.datx"), "Specify the ipip database filename for geo-IP lookups.")
		("http_doc", po::value<std::string>(&doc_dir)->value_name("doc"), "Specify the document root directory for the HTTP server.")
		("htpasswd", po::value<bool>(&htpasswd)->value_name("")->default_value(false, "false"), "Enable HTTP Basic Authentication (WWW-Authenticate) for the HTTP server.")

		("autoindex", po::value<bool>(&autoindex)->default_value(false, "false"), "Enable directory listing for the HTTP server.")
		("dns_upstream", po::value<std::string>(&dns_upstream)->value_name("ip:port/url"), "Specify the upstream DNS server for DoH (DNS over HTTPS).")
		("logs_path", po::value<std::string>(&log_dir)->value_name(""), "Specify the directory for log files.")

		("disable_logs", po::value<bool>(&disable_logs)->value_name("")->default_value(default_logs), "Disable logging output to the console.")
		("disable_http", po::value<bool>(&disable_http)->value_name("")->default_value(false, "false"), "Disable the HTTP proxy protocol.")
		("disable_socks", po::value<bool>(&disable_socks)->value_name("")->default_value(false, "false"), "Disable the SOCKS proxy protocol.")
		("disable_udp", po::value<bool>(&disable_udp)->value_name("")->default_value(false, "false"), "Disable the UDP proxy protocol.")
		("disable_insecure", po::value<bool>(&disable_insecure)->value_name("")->default_value(false, "false"), "Disable insecure (non-SSL) proxy protocols.")
		("disable_check_cert", po::value<bool>(&disable_check_cert)->value_name("")->default_value(false, "false"), "Disable TLS certificate verification.")

		("scramble", po::value<bool>(&scramble)->value_name("")->default_value(false, "false"), "Enable noise-based data obfuscation for enhanced security.")
		("noise_length", po::value<int64_t>(&noise_length)->value_name("length")->default_value(-1), "Length of the noise data in bytes (-1 = disable, 0-4095).")

		("asio_config", po::value<std::string>(&asio_config)->value_name("enable asio config env")->default_value("ASIO"), "Environment variable name for configuring Boost.Asio (default: ASIO).")

		("dns_udp_port", po::value<int>(&dns_udp_port)->default_value(0), "Listen UDP DNS requests on this port (accepted for launcher compatibility).")
		("dns_cache_size", po::value<int>(&dns_cache_size)->default_value(0), "DNS cache size (accepted for launcher compatibility).")
		("dns_cache_ttl", po::value<int>(&dns_cache_ttl)->default_value(0), "DNS cache TTL seconds (accepted for launcher compatibility).")
		("dns_no_ipv6", po::value<bool>(&dns_no_ipv6)->default_value(false, "false"), "Disable IPv6 DNS resolution (accepted for launcher compatibility).")
		("http2", po::value<bool>(&http2)->default_value(false, "false"), "Enable HTTP/2 (accepted for launcher compatibility).")

		("launcher", po::value<std::string>(&launcher_url)->value_name("ws://host:port/rpc?..."), "Launcher WebSocket control channel URL (internal, set by launcher).")
		("pid_file", po::value<std::string>(&pid_file)->value_name("path"), "Write process PID to this file (internal, set by launcher).")
	;

	// 解析命令行.
	// 兼容 launcher 生成的参数：其 ArgsFor 会把整数值按 float64 格式化，
	// 例如 --rate_limit 1.048576e+06（科学计数法），boost 的 int 解析不接受。
	// 这里先把整数选项的科学计数法数值预处理为普通整数。
	po::variables_map vm;

	try {
		const std::vector<std::string> int_opts = {
			"so_mark", "noise_length", "tcp_timeout", "udp_timeout",
			"rate_limit", "dns_udp_port", "dns_cache_size", "dns_cache_ttl",
		};
		auto is_int_opt = [&int_opts](const std::string& n) {
			return std::find(int_opts.begin(), int_opts.end(), n) != int_opts.end();
		};
		auto normalize_num = [](const std::string& s) -> std::string {
			if (s.find_first_of("eE") == std::string::npos)
				return s;
			char* end = nullptr;
			double v = std::strtod(s.c_str(), &end);
			if (end == s.c_str() || !std::isfinite(v))
				return s;
			return std::to_string(static_cast<long long>(v));
		};

		std::vector<std::string> normalized;
		normalized.reserve(argc);
		for (int i = 0; i < argc; i++)
		{
			std::string arg = argv[i];
			if (arg.rfind("--", 0) == 0)
			{
				auto eq = arg.find('=');
				if (eq != std::string::npos)
				{
					std::string name = arg.substr(2, eq - 2);
					if (is_int_opt(name))
						arg = arg.substr(0, eq + 1) + normalize_num(arg.substr(eq + 1));
				}
				else
				{
					std::string name = arg.substr(2);
					if (is_int_opt(name) && i + 1 < argc)
					{
						normalized.push_back(arg);
						normalized.push_back(normalize_num(argv[i + 1]));
						i++;
						continue;
					}
				}
			}
			normalized.push_back(arg);
		}

		po::store(
			po::command_line_parser(normalized)
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

	if (disable_logs && log_dir.empty())
	{
		xlogger::turnoff_logging();
	}
	else
	{
		if (log_dir.empty())
			xlogger::toggle_write_logging(false);
		else
			xlogger::init_logging(log_dir);

		// release 构建下控制台日志默认关闭，此处按 --disable_logs 显式控制，
		// 保证 launcher 传入 --disable_logs false 时能采集到 stdout/stderr 日志.
		xlogger::toggle_console_logging(!disable_logs);
	}

	print_args(argc, argv, vm);

	if (stdio_target.empty())
	{
		for (const auto& server_listen : server_listens)
			XLOG_DBG << "Start server: " << server_listen;
	}

	auto cfg = net::config_from_env(asio_config);
	net::io_context ioc(cfg);
	server_ptr server;
	net::signal_set terminator_signal(ioc);

#ifdef __linux__
	signal(SIGPIPE, SIG_IGN);
#endif

	if (stdio_target.empty())
	{
		terminator_signal.add(SIGINT);
		terminator_signal.add(SIGTERM);

#if defined(SIGQUIT)
		terminator_signal.add(SIGQUIT);
#endif // defined(SIGQUIT)

		terminator_signal.async_wait(
			[&](const boost::system::error_code&, int sig) mutable
			{
				terminator_signal.remove(sig);
				server->close();
				// server->close() 内部已停止 launcher 控制通道（关闭连接
				// 使 launcher 协程退出），否则 ioc.run() 因协程仍在运行而无法返回.
#ifndef NDEBUG
				std::thread([&] {
					std::this_thread::sleep_for(std::chrono::seconds(5));
					ioc.stop();
				}).detach();
#endif
			});
	}

	// 写入 pid 文件（供 launcher 重启后据此清理残余进程）；正常退出时删除.
	if (!pid_file.empty())
	{
		if (auto dir = fs::path(pid_file).parent_path(); !dir.empty() && dir != ".")
		{
			boost::system::error_code ec;
			fs::create_directories(dir, ec);
		}
		std::ofstream ofs(pid_file, std::ios::trunc);
		if (ofs)
			ofs << getpid() << "\n";
		else
			std::cerr << "write pid file " << pid_file << " error" << std::endl;
	}

	net::co_spawn(ioc, start_proxy_server(ioc, server), net::detached);

	ioc.run();

	// 删除 pid 文件（仅当内容仍是本进程 PID，避免竞态误删新进程文件）.
	if (!pid_file.empty())
	{
		std::ifstream ifs(pid_file);
		std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
		if (!content.empty() && std::stol(content) == static_cast<long>(getpid()))
		{
			boost::system::error_code ec;
			fs::remove(pid_file, ec);
		}
	}

	return EXIT_SUCCESS;
}
