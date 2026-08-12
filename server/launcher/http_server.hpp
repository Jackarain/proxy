//
// http_server.hpp
// ~~~~~~~~~~~~~~~
//
// launcher 的 HTTP 服务：REST API + WebUI 静态资源 + /rpc 控制通道。
// 支持 HTTPS（证书目录自动搜索 + SNI 多证书）。
// REST API 路由与 WebUI 静态资源服务。
//
// 并发模型：C++20 协程 + boost.asio。整个服务运行在单个 io_context 的
// 小线程池上（accept / 连接处理 / WebSocket JSON-RPC 会话全部协程化，
// 无每连接线程、无 poll）。WebUI 静态资源内嵌于可执行文件，从内存提供。
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef LAUNCHER_HTTP_SERVER_HPP
#define LAUNCHER_HTTP_SERVER_HPP

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/beast/http.hpp>

#include "manager.hpp"

namespace launcher {

// 一个证书条目。
struct cert_entry {
	std::string cert_file;
	std::string key_file;
	std::string domain;
	std::vector<std::string> sans;
};

// 从证书目录加载证书（递归遍历，配对 cert/key）。
bool load_certificates(const std::string& dir, std::vector<cert_entry>& entries, std::string& err);

// 构建服务端 SSL 上下文（SNI 多证书匹配）。
std::shared_ptr<boost::asio::ssl::context> build_ssl_context(const std::vector<cert_entry>& entries);

class http_server {
public:
	// webui 资源内嵌于可执行文件，不再需要 webui_dir。
	http_server(std::shared_ptr<manager> mgr,
		std::string webui_user, std::string webui_password, std::string version);
	~http_server();

	// 启动监听。listen_addr 形如 "0.0.0.0:18080"。
	// https 为 true 时使用 ssl_dir 中的证书。
	// 启动内部 io_context 线程池并协程化 accept 循环后立即返回。
	bool start(const std::string& listen_addr, bool https, const std::string& ssl_dir,
		std::string& err);
	void stop();

private:
	// 协程化的 accept 循环（运行在 ioc_ 线程池上）。
	boost::asio::awaitable<void> accept_loop();

	// 处理一个连接（plain / TLS），协程化。
	boost::asio::awaitable<void> handle_connection(boost::asio::ip::tcp::socket sock);

	// 服务一个 HTTP/WS 连接（模板，plain 与 TLS 共用），协程化。
	// /rpc 的 JSON-RPC 会话运行在共享 io_context 上。
	template <class Stream>
	boost::asio::awaitable<void> serve_http(Stream& stream);

	// 路由一个 HTTP 请求（从内嵌 WebUI 读取静态资源）。
	boost::beast::http::response<boost::beast::http::string_body>
	route(const boost::beast::http::request<boost::beast::http::string_body>& req);

	std::shared_ptr<manager> mgr_;
	std::string webui_user_;
	std::string webui_password_;
	std::string version_;

public:
	std::shared_ptr<manager> mgr() const { return mgr_; }
	const std::string& webui_user() const { return webui_user_; }
	const std::string& webui_password() const { return webui_password_; }

	// 共享 io_context：accept / 连接处理 / JSON-RPC 会话全部运行于此。
	boost::asio::io_context ioc_;
	std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
	std::shared_ptr<boost::asio::ssl::context> ssl_ctx_;
	std::atomic<bool> stopped_{ false };
	// ioc_ 的工作线程（小线程池）。
	std::vector<std::thread> threads_;
};

} // namespace launcher

#endif // LAUNCHER_HTTP_SERVER_HPP
