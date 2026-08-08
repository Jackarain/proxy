//
// http_server.hpp
// ~~~~~~~~~~~~~~~
//
// launcher 的 HTTP 服务：REST API + WebUI 静态资源 + /rpc 控制通道。
// 支持 HTTPS（与 golang 版本相同的证书目录自动搜索 + SNI 多证书）。
// 与 golang 版本 internal/launcher/api.go 的路由与响应格式一致。
//
// 并发模型：每个连接在独立线程 + 独立 io_context 中处理；
// /rpc 控制通道使用 tinyrpc jsonrpc_session 运行在该连接的 io_context 上。
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
	http_server(std::shared_ptr<manager> mgr, std::string webui_dir,
		std::string webui_user, std::string webui_password, std::string version);
	~http_server();

	// 启动监听。listen_addr 形如 "0.0.0.0:18080"。
	// https 为 true 时使用 ssl_dir 中的证书。
	bool start(const std::string& listen_addr, bool https, const std::string& ssl_dir,
		std::string& err);
	void stop();

private:
	void accept_loop();

	// 处理一个连接（plain / TLS）：创建连接级 io_context 并迁移 socket。
	void handle_connection(std::shared_ptr<boost::asio::ip::tcp::socket> sock);

	// 服务一个 HTTP/WS 连接（模板，plain 与 TLS 共用）。
	// conn_ioc 为本连接专属 io_context（运行在连接线程中），
	// /rpc 的 JSON-RPC 会话运行在此 io_context 上。
	template <class Stream>
	void serve_http(boost::asio::io_context& conn_ioc, Stream& stream);

	// 路由一个 HTTP 请求（可能阻塞，运行在连接线程）。
	boost::beast::http::response<boost::beast::http::string_body>
	route(const boost::beast::http::request<boost::beast::http::string_body>& req);

	std::shared_ptr<manager> mgr_;
	std::string webui_dir_;
	std::string webui_user_;
	std::string webui_password_;
	std::string version_;

public:
	std::shared_ptr<manager> mgr() const { return mgr_; }
	const std::string& webui_user() const { return webui_user_; }
	const std::string& webui_password() const { return webui_password_; }

	boost::asio::io_context ioc_;
	std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
	std::shared_ptr<boost::asio::ssl::context> ssl_ctx_;
	std::atomic<bool> stopped_{ false };
	std::thread accept_thread_;
};

} // namespace launcher

#endif // LAUNCHER_HTTP_SERVER_HPP
