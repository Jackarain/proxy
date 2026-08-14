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
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#ifndef _WIN32
#include <unistd.h>
#endif

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include "httpc/httpc.hpp"
#include "proxy/default_cert.hpp"
#include "unzip.h"

#include "http_server.hpp"
#include "manager.hpp"
#include "version.hpp"

namespace fs = boost::filesystem;
namespace po = boost::program_options;

using namespace launcher;

namespace {

// 本地找不到 proxy_server 时，自动下载解压的地址与目录。
static constexpr const char* kProxyServerDownloadUrl =
	"https://nightly.link/Jackarain/proxy/workflows/Build/master/proxy_server-alpine_musl_x64.zip";
static constexpr const char* kProxyServerDownloadZip = "/tmp/proxy_server.zip";
static constexpr const char* kProxyServerDownloadedExe = "/tmp/proxy_server";

// 解压 proxy_server.zip 到其所在目录（/tmp）。
bool unzip_proxy_server(const std::string& download_path)
{
	unzFile uf = unzOpen64(download_path.c_str());
	if (uf == nullptr) {
		std::fprintf(stderr, "[错误] 无法打开压缩包: %s\n", download_path.c_str());
		return false;
	}

	// 解压目标目录为压缩包所在目录（/tmp）。
	fs::path target_dir = fs::path(download_path).parent_path();
	if (target_dir.empty())
		target_dir = ".";

	// 确保目标目录存在。
	{
		boost::system::error_code unused_ec;
		fs::create_directories(target_dir, unused_ec);
	}

	constexpr std::size_t kBufferSize = 8192;
	std::vector<char> buf(kBufferSize);

	int err = unzGoToFirstFile(uf);
	if (err != UNZ_OK && err != UNZ_END_OF_LIST_OF_FILE) {
		std::fprintf(stderr, "[错误] 读取压缩包失败: %d\n", err);
		unzClose(uf);
		return false;
	}

	do {
		char filename_inzip[65536 + 1];
		unz_file_info64 file_info;

		err = unzGetCurrentFileInfo64(uf, &file_info,
			filename_inzip, sizeof(filename_inzip),
			nullptr, 0, nullptr, 0);
		if (err != UNZ_OK) {
			std::fprintf(stderr, "[错误] 读取压缩包文件信息失败: %d\n", err);
			break;
		}

		// 构建输出路径。
		fs::path output_path = target_dir / filename_inzip;
		std::fprintf(stderr, "[info] 解压文件: %s\n", output_path.string().c_str());

		// 检查路径遍历攻击: 确保解压路径在 target_dir 范围内。
		try {
			auto canonical_path = fs::absolute(output_path).lexically_normal();
			auto canonical_target = fs::absolute(target_dir).lexically_normal();
			if (canonical_path.string().find(canonical_target.string()) != 0) {
				std::fprintf(stderr, "[warn] 跳过路径越界文件: %s\n", filename_inzip);
				err = unzGoToNextFile(uf);
				if (err == UNZ_END_OF_LIST_OF_FILE)
					break;
				continue;
			}
		} catch (...) {
			std::fprintf(stderr, "[warn] 跳过路径错误文件: %s\n", filename_inzip);
			err = unzGoToNextFile(uf);
			if (err == UNZ_END_OF_LIST_OF_FILE)
				break;
			continue;
		}

		// 检查是否为目录（文件名以 '/' 或 '\\' 结尾）。
		std::size_t name_len = std::strlen(filename_inzip);
		if (name_len > 0 &&
			(filename_inzip[name_len - 1] == '/' ||
			 filename_inzip[name_len - 1] == '\\')) {
			// 创建目录。
			boost::system::error_code unused_ec;
			fs::create_directories(output_path, unused_ec);
		} else {
			// 创建父目录。
			{
				boost::system::error_code unused_ec;
				fs::create_directories(output_path.parent_path(), unused_ec);
			}

			// 打开当前文件进行读取。
			err = unzOpenCurrentFile(uf);
			if (err != UNZ_OK) {
				std::fprintf(stderr, "[错误] 打开压缩包内文件失败: %s, err: %d\n",
					filename_inzip, err);
				err = unzGoToNextFile(uf);
				if (err == UNZ_END_OF_LIST_OF_FILE)
					break;
				continue;
			}

			// 创建输出文件。
			FILE* fout = std::fopen(output_path.string().c_str(), "wb");
			if (fout == nullptr) {
				std::fprintf(stderr, "[错误] 创建输出文件失败: %s\n",
					output_path.string().c_str());
				unzCloseCurrentFile(uf);
				err = unzGoToNextFile(uf);
				if (err == UNZ_END_OF_LIST_OF_FILE)
					break;
				continue;
			}

			// 读取并写入。
			do {
				int read_bytes = unzReadCurrentFile(uf, buf.data(), (unsigned)kBufferSize);
				if (read_bytes < 0) {
					std::fprintf(stderr, "[错误] 读取压缩包内文件失败: %s, err: %d\n",
						filename_inzip, read_bytes);
					break;
				}
				if (read_bytes == 0)
					break;
				if (std::fwrite(buf.data(), 1, read_bytes, fout) != (std::size_t)read_bytes) {
					std::fprintf(stderr, "[错误] 写入文件失败: %s\n",
						output_path.string().c_str());
					break;
				}
			} while (true);

			std::fclose(fout);

			// 关闭当前文件。
			int close_err = unzCloseCurrentFile(uf);
			if (close_err == UNZ_CRCERROR) {
				std::fprintf(stderr, "[warn] 文件 CRC 校验失败: %s\n", filename_inzip);
			}

			// 设置文件权限为可执行（对于 Unix 系统）。
#if !defined(_WIN32)
			boost::system::error_code perm_ec;
			fs::permissions(output_path,
				fs::owner_read | fs::owner_write | fs::owner_exe |
				fs::group_read | fs::group_exe |
				fs::others_read | fs::others_exe,
				perm_ec);
#endif
		}

		err = unzGoToNextFile(uf);
		if (err == UNZ_END_OF_LIST_OF_FILE)
			break;
		if (err != UNZ_OK) {
			std::fprintf(stderr, "[错误] 跳转压缩包内下一文件失败: %d\n", err);
			break;
		}
	} while (true);

	unzClose(uf);
	return true;
}

// 通过 httpc 下载 proxy_server 压缩包；成功返回 true。
net::awaitable<bool> download_proxy_server(net::io_context& ioc,
	const std::string& download_path)
{
	httpc::http_client client(ioc.get_executor(),
		net::buffer(default_root_certificates()));
	httpc::http_request req;
	req.method(httpc::verb::get);
	client.set_download_file(download_path);
	client.max_redirects(10);
	client.user_agent("curl/8.21.0");

	auto result = co_await client.async_perform(kProxyServerDownloadUrl, req);
	if (result)
		co_return true;

	std::fprintf(stderr, "\x1b[31m[错误] 下载 proxy_server 失败: %s\x1b[0m\n",
		result.error().message().c_str());
	co_return false;
}

// 在系统 $PATH 中查找可执行文件；找到返回绝对路径，未找到返回空字符串。
std::string find_in_path(const std::string& exe)
{
	const char* path_env = std::getenv("PATH");
	if (path_env == nullptr || *path_env == '\0')
		return {};

#ifdef _WIN32
	const char path_sep = ';';
#else
	const char path_sep = ':';
#endif

	// Windows 下按 PATHEXT 依次尝试可执行文件扩展名。
	std::vector<std::string> exts;
#ifdef _WIN32
	{
		const char* pathext_env = std::getenv("PATHEXT");
		std::string pathext = (pathext_env != nullptr && *pathext_env != '\0')
			? pathext_env : ".COM;.EXE;.BAT;.CMD";
		std::size_t pos = 0;
		while (pos <= pathext.size()) {
			std::size_t end = pathext.find(';', pos);
			std::string ext = pathext.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
			if (!ext.empty())
				exts.push_back(std::move(ext));
			if (end == std::string::npos)
				break;
			pos = end + 1;
		}
	}
#else
	exts.emplace_back();
#endif

	std::string path = path_env;
	std::size_t pos = 0;
	while (pos <= path.size()) {
		std::size_t end = path.find(path_sep, pos);
		std::string dir = path.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
		for (const auto& ext : exts) {
			fs::path candidate = fs::absolute(fs::path(dir) / (exe + ext));
			boost::system::error_code ec;
			if (fs::is_regular_file(candidate, ec)) {
#ifndef _WIN32
				if (::access(candidate.c_str(), X_OK) == 0)
#endif
					return candidate.string();
			}
		}
		if (end == std::string::npos)
			break;
		pos = end + 1;
	}
	return {};
}

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

	// 解析 proxy_server 可执行文件路径。
	// 未显式指定 --proxy_server 时：优先当前目录下的 proxy_server，其次在系统 $PATH 中查找，
	// 全部找不到时自动从 nightly.link 下载并解压到 /tmp 使用。
	if (vm["proxy_server"].defaulted()) {
		boost::system::error_code ec;
		if (!fs::exists(proxy_path, ec)) {
			std::string found = find_in_path("proxy_server");
			if (!found.empty()) {
				proxy_path = std::move(found);
			} else {
				// 全部找不到：优先复用 /tmp 下已下载的 proxy_server，否则自动下载。
				proxy_path = kProxyServerDownloadedExe;
				if (!fs::is_regular_file(proxy_path, ec)) {
					std::fprintf(stderr, "[info] 未找到本地 proxy_server，正在从 nightly.link 下载...\n");
					net::io_context ioc;
					bool ok = false;
					net::co_spawn(ioc, [&]() -> net::awaitable<void> {
						ok = co_await download_proxy_server(ioc, kProxyServerDownloadZip);
					}, net::detached);
					ioc.run();
					if (!ok || !unzip_proxy_server(kProxyServerDownloadZip)) {
						std::fprintf(stderr, "\x1b[31m[错误] 自动获取 proxy_server 失败，"
							"请将 proxy_server 放到 launcher 的当前目录或系统 $PATH 中，"
							"或使用 --proxy_server 指定其路径。\x1b[0m\n");
						return 1;
					}
					std::fprintf(stderr, "[info] proxy_server 已下载并解压到 %s\n",
						proxy_path.c_str());
				}
			}
		}
	}

	// 检查 proxy_server 可执行文件是否存在。
	{
		boost::system::error_code ec;
		if (!fs::exists(proxy_path, ec)) {
			std::fprintf(stderr, "\x1b[31m[错误] 未找到 proxy_server 可执行文件: %s\x1b[0m\n", proxy_path.c_str());
			std::fprintf(stderr, "请将 proxy_server 放到 launcher 的当前目录或系统 $PATH 中，或使用 --proxy_server 指定其路径。\n");
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
