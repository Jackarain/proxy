//
// main.cpp
// ~~~~~~~~
//
// launcher 是 gproxy 的 WebUI 管理器（C++ 版本，与 golang 版本协议完全兼容）：
// 负责创建/启停多个 proxy_server 进程实例，并通过 WebSocket + JSON-RPC
// 实现运行期热改配置与实时状态展示。
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <boost/filesystem.hpp>

#include "http_server.hpp"
#include "manager.hpp"
#include "version.hpp"

namespace fs = boost::filesystem;

using namespace launcher;

namespace {

// 命令行选项定义（与 golang 版本 cmd/launcher 一致）。
struct option {
	std::string name;
	std::string typ; // string / bool
	std::string help;
	bool applied = false;
	std::string str_value;
	bool bool_value = false;
};

std::vector<option> make_options() {
	return {
		{ "help", "bool", "Show this help message and exit." },
		{ "listen", "string", "WebUI HTTP listen address (default: 0.0.0.0:18080)." },
		{ "ssl_certificate_dir", "string", "Directory containing SSL/TLS certificates for HTTPS WebUI (auto-search, same as proxy_server)." },
		{ "proxy_server", "string", "Path to the proxy_server executable (default: ./proxy_server)." },
		{ "data_dir", "string", "Directory to persist instance configurations (default: launcher_data)." },
		{ "webui_user", "string", "Optional WebUI basic-auth username." },
		{ "webui_password", "string", "Optional WebUI basic-auth password." },
		{ "no_kill_on_exit", "bool", "Do not stop proxy_server instances when launcher exits." },
	};
}

option* find_option(std::vector<option>& opts, const std::string& name) {
	for (auto& o : opts)
		if (o.name == name)
			return &o;
	return nullptr;
}

bool parse_bool(const std::string& s) {
	std::string lower = s;
	std::transform(lower.begin(), lower.end(), lower.begin(),
		[](unsigned char c) { return std::tolower(c); });
	return lower == "true" || lower == "1" || lower == "yes" || lower == "on";
}

bool is_bool_val(const std::string& s) {
	std::string lower = s;
	std::transform(lower.begin(), lower.end(), lower.begin(),
		[](unsigned char c) { return std::tolower(c); });
	return lower == "true" || lower == "false" || lower == "1" || lower == "0" ||
		lower == "yes" || lower == "no" || lower == "on" || lower == "off";
}

void print_help(const std::vector<option>& opts) {
	std::printf("Usage: launcher [options]\n");
	std::printf("Options:\n");
	for (const auto& o : opts)
		std::printf("  --%-22s %s\n", o.name.c_str(), o.help.c_str());
}

// 解析监听地址与端口。
bool parse_listen_addr(const std::string& listen_addr, std::string& host, int& port) {
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

// 解析 webui 目录：优先可执行文件目录，其次当前目录。
std::string resolve_webui_dir(const std::string& exe_path) {
	auto try_dir = [](const std::string& base) -> std::string {
		fs::path p = fs::path(base) / "webui";
		boost::system::error_code ec;
		if (fs::is_directory(p, ec))
			return p.string();
		return {};
	};
	if (!exe_path.empty()) {
		fs::path exe(exe_path);
		if (exe.has_parent_path()) {
			auto d = try_dir(exe.parent_path().string());
			if (!d.empty())
				return d;
		}
	}
	{
		auto d = try_dir(fs::current_path().string());
		if (!d.empty())
			return d;
	}
	return {};
}

std::atomic<bool> g_stopping{ false };

} // namespace

int main(int argc, char** argv) {
#ifdef __linux__
	signal(SIGPIPE, SIG_IGN);
#endif

	auto opts = make_options();

	// 命令行为空时显示帮助。
	if (argc == 1) {
		print_help(opts);
		return 0;
	}

	// 解析命令行（与 golang 版本一致）。
	for (int i = 1; i < argc; i++) {
		std::string arg = argv[i];
		if (arg.rfind("--", 0) != 0)
			continue;
		std::string name = arg.substr(2);
		std::string val;
		bool has_val = false;
		auto eq = name.find('=');
		if (eq != std::string::npos) {
			val = name.substr(eq + 1);
			name = name.substr(0, eq);
			has_val = true;
		}
		option* o = find_option(opts, name);
		if (!o)
			continue;
		if (o->typ == "bool") {
			if (!has_val && i + 1 < argc &&
				std::string(argv[i + 1]).rfind("--", 0) != 0 &&
				is_bool_val(argv[i + 1])) {
				val = argv[i + 1];
				has_val = true;
				i++;
			}
			if (!has_val)
				val = "true";
			o->bool_value = parse_bool(val);
			o->applied = true;
			continue;
		}
		if (!has_val) {
			if (i + 1 < argc && std::string(argv[i + 1]).rfind("--", 0) != 0) {
				val = argv[i + 1];
				has_val = true;
				i++;
			} else {
				val = "";
			}
		}
		o->str_value = val;
		o->applied = true;
	}

	auto get_str = [&opts](const std::string& name, const std::string& def) -> std::string {
		option* o = find_option(opts, name);
		return (o && o->applied) ? o->str_value : def;
	};
	auto get_bool = [&opts](const std::string& name, bool def) -> bool {
		option* o = find_option(opts, name);
		return (o && o->applied) ? o->bool_value : def;
	};

	if (get_bool("help", false)) {
		print_help(opts);
		return 0;
	}

	std::string listen_addr = get_str("listen", "0.0.0.0:18080");
	std::string ssl_dir = get_str("ssl_certificate_dir", "");
	std::string proxy_path = get_str("proxy_server", "./proxy_server");
	std::string data_dir = get_str("data_dir", "launcher_data");
	std::string webui_user = get_str("webui_user", "");
	std::string webui_password = get_str("webui_password", "");
	bool no_kill_on_exit = get_bool("no_kill_on_exit", false);

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
	std::string webui_dir = resolve_webui_dir(argv[0]);
	if (webui_dir.empty())
		std::fprintf(stderr, "[warn] webui directory not found, WebUI pages will be unavailable\n");

	// 实例管理器。
	auto mgr = std::make_shared<manager>(data_dir, proxy_path, work_dir.string());
	// 必须先设置 WS 地址再 Load：Load 会自动启动 autostart 实例，
	// 此时需要正确的地址/端口来构造 --launcher 控制通道 URL。
	bool https = !ssl_dir.empty();
	mgr->set_ws_addr(host, port, https);

	// HTTP 服务。
	http_server server(mgr, webui_dir, webui_user, webui_password, build_version());
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

	// 信号处理：停止所有实例后退出。
	signal(SIGINT, [](int) { g_stopping = true; });
	signal(SIGTERM, [](int) { g_stopping = true; });

	while (!g_stopping)
		std::this_thread::sleep_for(std::chrono::milliseconds(200));

	std::fprintf(stderr, "[info] received signal, shutting down\n");
	if (!no_kill_on_exit) {
		for (const auto& id : mgr->ids()) {
			std::string err;
			if (!mgr->stop(id, err))
				std::fprintf(stderr, "[warn] stop instance %s: %s\n", id.c_str(), err.c_str());
		}
	}
	server.stop();
	return 0;
}
