//
// http_server.hpp
// ~~~~~~~~~~~~~~~
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// launcher 的 HTTP 服务：REST API + WebUI 静态资源 + /rpc 控制通道。
// 支持 HTTPS（证书目录自动搜索 + SNI 多证书）。服务运行在单个 io_context
// 的小线程池上，WebUI 静态资源内嵌于可执行文件。
//

#ifndef LAUNCHER_HTTP_SERVER_HPP
#define LAUNCHER_HTTP_SERVER_HPP

#include <atomic>
#include <chrono>
#include <ctime>
#include <memory>
#include <mutex>
#include <set>
#include <string>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/beast/http.hpp>

#include "manager.hpp"

namespace launcher {

namespace net = boost::asio;

// 一个证书条目。
struct cert_entry
{
	std::string cert_file_;
	std::string key_file_;
	std::string domain_;
	std::vector<std::string> sans_;
	std::time_t expire_ = 0; // 证书过期时间 (Unix 秒), 0 表示未知.
};

// 从证书目录加载证书（递归遍历，配对 cert/key）。
bool load_certificates(const std::string& dir, std::vector<cert_entry>& entries, std::string& err);

// 构建服务端 SSL 上下文（SNI 多证书匹配）。
// SNI 回调所需的证书数据由返回的 shared_ptr 的自定义 deleter 持有，
// 生命周期与 context 一致，调用方无需另行维护。
std::shared_ptr<net::ssl::context> build_ssl_context(const std::vector<cert_entry>& entries);

class http_server
{
public:
	// ioc 由 main 持有（服务与实例生命周期共用同一 io_context），此处仅引用。
	http_server(std::shared_ptr<manager> mgr, net::io_context& ioc,
		std::string webui_user, std::string webui_password, std::string version);
	~http_server();

	// 启动监听。listen_addr 形如 "0.0.0.0:18080"。
	// https 为 true 时使用 ssl_dir 中的证书。启动协程化 accept 循环后立即返回。
	bool start(const std::string& listen_addr, bool https, const std::string& ssl_dir,
		std::string& err);

	// 关闭监听与活动连接，使挂起的协程自然退出（不停止 ioc）。
	void stop();

private:
	// 协程化的 accept 循环（挂在共享 io_context 上）。
	net::awaitable<void> accept_loop();

	// 处理一个连接（plain / TLS），协程化。
	net::awaitable<void> handle_connection(net::ip::tcp::socket sock);

	// 服务一个 HTTP/WS 连接（模板，plain 与 TLS 共用），协程化。
	// /rpc 的 JSON-RPC 会话运行在共享 io_context 上。
	template <class Stream>
	net::awaitable<void> serve_http(Stream& stream);

	// 路由一个 HTTP 请求（协程；涉及 RPC 的操作异步等待 proxy_server 响应）。
	// 静态资源从内嵌 WebUI 读取。
	net::awaitable<boost::beast::http::response<boost::beast::http::string_body>>
	route(const boost::beast::http::request<boost::beast::http::string_body>& req);

	// 活动连接登记/注销（stop 时关闭使协程退出）。
	void add_conn(net::ip::tcp::socket::native_handle_type h);
	void remove_conn(net::ip::tcp::socket::native_handle_type h);

	// 扫描证书目录检查过期；存在过期证书时热更新 SSL 上下文.
	// 返回距下次检查的间隔, 0 表示存在过期证书且已热更新.
	std::chrono::seconds certificate_check();

	// 证书过期自动热更新协程（https 模式下启动, 参考 proxy_server 的
	// certificate_check/tick 实现: 过期后以不超过 2 分钟的周期持续重试,
	// 直到证书不再过期）.
	net::awaitable<void> certificate_check_loop();

	std::shared_ptr<manager> m_mgr_;
	net::io_context& m_ioc_;
	std::string m_webui_user_;
	std::string m_webui_password_;
	std::string m_version_;
	std::unique_ptr<net::ip::tcp::acceptor> m_acceptor_;
	std::shared_ptr<net::ssl::context> m_ssl_ctx_;
	// 证书目录（https 模式, 供证书过期热更新扫描）.
	std::string m_ssl_dir_;
	// 保护 m_ssl_ctx_ 的访问与热更新切换.
	std::mutex m_ssl_mu_;
	std::atomic<bool> m_stopped_{ false };

	std::mutex m_conn_mu_;
	std::set<net::ip::tcp::socket::native_handle_type> m_conns_;

public:
	std::shared_ptr<manager> mgr() const { return m_mgr_; }
	const std::string& webui_user() const { return m_webui_user_; }
	const std::string& webui_password() const { return m_webui_password_; }
};

} // namespace launcher

#endif // LAUNCHER_HTTP_SERVER_HPP
