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
bool noise_injection = false;
bool disable_logs;
bool autoindex = false;

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
	opt.noise_injection_ = noise_injection;
	opt.local_ip_ = local_ip;

	opt.reuse_port_ = reuse_port;
	opt.happyeyeballs_ = happyeyeballs;

	opt.doc_directory_ = doc_directory;
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
		("autoindex", po::value<bool>(&autoindex)->default_value(false), "Enable directory listing.")
		("logs_path", po::value<std::string>(&log_directory)->value_name(""), "Specify directory for log files.")
		("disable_logs", po::value<bool>(&disable_logs)->value_name(""), "Disable logging.")
		("disable_http", po::value<bool>(&disable_http)->value_name("")->default_value(false), "Disable HTTP protocol.")
		("disable_socks", po::value<bool>(&disable_socks)->value_name("")->default_value(false), "Disable SOCKS proxy protocol.")
		// ("noise_injection", po::value<bool>(&noise_injection)->value_name("")->default_value(false), "Enable RLH(random-length headers) protocol.")
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


#include <random>
#include <vector>
#include <cstdint>
#include <climits>
#include <iostream>
#include <iomanip>

int start_position(std::mt19937& gen)
{
	const static int pos[] =
	{ 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24 };

	static std::normal_distribution<> dis(5, 4);

	int num = static_cast<int>(dis(gen));
	if (num < 0) num = 0;
	if (num > 10) num = 10;

	return pos[num];
}

inline std::vector<uint8_t>
generate_noise(uint16_t max_len = 0x7FFF, std::set<uint8_t> bfilter = {})
{
	if (max_len <= 4)
		return {};

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    std::vector<uint8_t> data;

	uint8_t bhalf = static_cast<uint8_t>(max_len >> 8);
	uint8_t ehalf = static_cast<uint8_t>(max_len & 0xFF);

	int start_pos = start_position(gen);
	if (start_pos > max_len)
		start_pos = (max_len / 2) * 2;

	bool flip = false;
	for (int i = 0; i < start_pos; i++)
	{
		uint8_t c = static_cast<uint8_t>(dis(gen));

		if (flip)
			c |= bhalf;
		else
			c |= ehalf;

		if (i == 0 && !bfilter.empty())
		{
			while (bfilter.find(c) != bfilter.end())
			{
				c = static_cast<uint8_t>(dis(gen));
				if (flip)
					c |= bhalf;
				else
					c |= ehalf;
			}
		}

		flip = !flip;

		data.push_back(c);
	}

	uint16_t min_len = std::min<uint16_t>(max_len, static_cast<uint16_t>(start_pos));
	if (min_len >= max_len)
		min_len = max_len - 1;

	auto length = std::uniform_int_distribution<>(min_len, max_len - 1)(gen);

	data[start_pos - 2] = static_cast<uint8_t>(length >> 8);
	data[start_pos - 1] = static_cast<uint8_t>(length & 0xFF);

	data[start_pos - 4] |= static_cast<uint8_t>(min_len >> 8);
	data[start_pos - 3] |= static_cast<uint8_t>(min_len & 0xFF);

	uint16_t a = data[start_pos - 3] | (data[start_pos - 4] << CHAR_BIT);
	uint16_t b = data[start_pos - 1] | (data[start_pos - 2] << CHAR_BIT);

	length = a & b;

    while (data.size() < length) {
        data.push_back(static_cast<uint8_t>(dis(gen)));
    }

    return data;
}

void printVectorAsUint16(const std::vector<uint8_t>& data, uint16_t target_len = 0x7FFF)
{
    if (data.size() < 4) {
        std::cerr << "数据长度小于4，无法转换为 uint16_t。" << std::endl;
        return;
    }

    std::cout << "print len: " << data.size() << "\n";

    uint16_t p1 = 0;
    p1 = data[0] | (data[1] << 8);
    bool found = false;
    int count = 0;
    // 从第二个字节开始，确保每个 uint16_t 的前一个字节是前一个 uint16_t 的后一个字节
    for (size_t i = 1; i < data.size(); i += 2) {
        // 组合当前字节和前一个字节为 uint16_t
        uint16_t value = static_cast<uint16_t>(data[i]) | (static_cast<uint16_t>(data[i - 1]) << 8);

        // 设置字段宽度和左对齐
        std::cout << std::left << std::setw(10) << value;

        if ((p1 & value) < target_len && !found && i != 1) {
            found = true;
            std::cout << "decode len: " << (p1 & value) << "\n";
        }

        p1 = value;

        count++;
        if (count == 8) { // 每输出8个数值后换行
            std::cout << std::endl;
            count = 0;
        }
    }

    if (count > 0) {
        std::cout << std::endl; // 保证最后一行在输出后有换行
    }
}

int main2() {
	uint16_t max_len = 0x001F;
	auto result = generate_noise(max_len);
	printVectorAsUint16(result, max_len);
}
