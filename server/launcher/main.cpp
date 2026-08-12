//
// main.cpp
// ~~~~~~~~
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// launcher 入口：创建/启停多个 proxy_server 进程实例，并通过
// WebSocket + JSON-RPC 实现运行期热改配置与实时状态展示。
//

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include "http_server.hpp"
#include "manager.hpp"
#include "version.hpp"

namespace fs = boost::filesystem;
namespace po = boost::program_options;

using namespace launcher;

namespace {

// 解析监听地址与端口。
bool parse_listen_addr(const std::string& listen_addr, std::string& host, int& port)
{
	auto colon = listen_addr.rfind(':');
	if (colon == std::string::npos)
		return false;
	host = listen_addr.substr(0, colon);
	std::string port_str = listen_addr.substr(colon + 1);
	try {
		port = std::stoi(port_str);
	} catch (...) {
		return false;
	}
	return port > 0 && port <= 65535;
}

} // namespace

int main(int argc, char** argv)
{
#ifdef __linux__
	signal(SIGPIPE, SIG_IGN);
#endif

	std::string listen_addr = "0.0.0.0:18080";
	std::string ssl_dir;
	std::string proxy_path = "./proxy_server";
	std::string data_dir = "launcher_data";
	std::string webui_user;
	std::string webui_password;
	bool no_kill_on_exit = false;

	po::options_description desc("Options");
	desc.add_options()
		("help,h", "Show this help message and exit.")
		("listen", po::value<std::string>(&listen_addr)
			->default_value("0.0.0.0:18080")->value_name("addr"),
			"WebUI HTTP listen address.")
		("ssl_certificate_dir", po::value<std::string>(&ssl_dir)
			->value_name("path"),
			"Directory containing SSL/TLS certificates for HTTPS WebUI (auto-search, same as proxy_server).")
		("proxy_server", po::value<std::string>(&proxy_path)
			->default_value("./proxy_server")->value_name("path"),
			"Path to the proxy_server executable.")
		("data_dir", po::value<std::string>(&data_dir)
			->default_value("launcher_data")->value_name("dir"),
			"Directory to persist instance configurations.")
		("webui_user", po::value<std::string>(&webui_user)
			->value_name("user"),
			"Optional WebUI basic-auth username.")
		("webui_password", po::value<std::string>(&webui_password)
			->value_name("password"),
			"Optional WebUI basic-auth password.")
		("no_kill_on_exit", po::value<bool>(&no_kill_on_exit)
			->default_value(false, "false")->value_name(""),
			"Do not stop proxy_server instances when launcher exits.")
		;

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
	} catch (const po::error& e) {
		std::fprintf(stderr, "Error parsing command line: %s\n", e.what());
		return EXIT_FAILURE;
	}

	// 帮助输出（无参数时也显示帮助）。
	if (vm.count("help") || argc == 1) {
		std::cout << "Usage: launcher [options]\n\n" << desc << "\n";
		return EXIT_SUCCESS;
	}

	// 检查 proxy_server 可执行文件是否存在。
	{
		boost::system::error_code ec;
		if (!fs::exists(proxy_path, ec)) {
			std::fprintf(stderr, "\x1b[31m[错误] 未找到 proxy_server 可执行文件: %s\x1b[0m\n", proxy_path.c_str());
			std::fprintf(stderr, "请将 proxy_server 放到 launcher 的当前目录下，或使用 --proxy_server 指定其路径。\n");
			return 1;
		}
	}

	// 解析监听地址与端口（端口用于生成传给 proxy_server 的控制通道地址）。
	std::string host;
	int port = 0;
	if (!parse_listen_addr(listen_addr, host, port)) {
		std::fprintf(stderr, "invalid --listen address: %s\n", listen_addr.c_str());
		return 1;
	}

	fs::path work_dir = fs::current_path();

	// 实例管理器。
	auto mgr = std::make_shared<manager>(data_dir, proxy_path, work_dir.string());
	// 必须先设置 WS 地址再 Load：Load 会自动启动 autostart 实例，
	// 此时需要正确的地址/端口来构造 --launcher 控制通道 URL。
	bool https = !ssl_dir.empty();
	mgr->set_ws_addr(host, port, https);

	// 共享 io_context：HTTP 服务与实例 RPC 全部运行于此（main 持有）。
	net::io_context ioc;

	// HTTP 服务（WebUI 静态资源内嵌于可执行文件，从内存提供）。
	http_server server(mgr, ioc, webui_user, webui_password, build_version());
	std::string err;
	if (!server.start(listen_addr, https, ssl_dir, err)) {
		std::fprintf(stderr, "%s\n", err.c_str());
		return 1;
	}

	std::fprintf(stderr, "[info] launcher started, WebUI %s://%s (control channel port %d)\n",
		https ? "https" : "http", listen_addr.c_str(), port);
	if (!webui_user.empty())
		std::fprintf(stderr, "[info] WebUI basic auth enabled for user %s\n", webui_user.c_str());

	// 加载实例配置并自动拉起 autostart 实例。
	if (!mgr->load()) {
		std::fprintf(stderr, "load instances data failed\n");
		return 1;
	}

	// 信号处理：关闭 HTTP 服务、异步停止所有实例后结束 io_context。
	net::signal_set terminator(ioc);
	terminator.add(SIGINT);
	terminator.add(SIGTERM);
	terminator.async_wait([&](const boost::system::error_code&, int) {
		terminator.remove(SIGINT);
		terminator.remove(SIGTERM);
		std::fprintf(stderr, "[info] received signal, shutting down\n");
		server.stop();
		if (!no_kill_on_exit) {
			// 异步停止所有实例；完成后 stop 掉 io_context。
			// 控制通道关闭后 RPC 调用可能无法取消（后台协程挂起），
			// 因此需要显式停止 io_context 才能让 run() 返回。
			net::co_spawn(ioc, [mgr, &ioc]() -> net::awaitable<void> {
				for (const auto& id : mgr->ids()) {
					std::string err;
					co_await mgr->stop(id, err);
				}
				ioc.stop();
			}, net::detached);
		} else {
			ioc.stop();
		}
	});

	// 单线程运行 io_context。
	ioc.run();
	return 0;
}
