//
// doh_client.hpp
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef INCLUDE__2026_08_25__DOH_CLIENT_HPP
#define INCLUDE__2026_08_25__DOH_CLIENT_HPP

#include "proxy/proxy_session.hpp"
#include "proxy/proxy_stream.hpp"
#include "proxy/proxy_util.hpp"
#include "proxy/logging.hpp"

#include <boost/asio/steady_timer.hpp>

#include <chrono>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace proxy {

	//////////////////////////////////////////////////////////////////////////
	// DoH 客户端共享辅助函数（供 doh_client 与 tun_server 复用）.
	// 原为 tun_server.cpp 匿名命名空间内部实现，提取至此.

	// proxy_pass 是否使用 SSL 加密（https/wss 或显式配置 proxy_pass_ssl）.
	inline bool proxy_use_ssl(const urls::url& url,
		const proxy_server_option& opt) noexcept
	{
		if (opt.proxy_pass_use_ssl_)
			return true;
		auto scheme = boost::to_lower_copy(std::string(url.scheme()));
		return scheme.ends_with("s");
	}

	// 配置了 SO_MARK 时对 socket 应用标记（配合策略路由防止环路）.
	template <typename Socket>
	void apply_so_mark_if(Socket& sock,
		const proxy_server_option& opt) noexcept
	{
		if (!opt.so_mark_)
			return;

		auto ret = apply_so_mark(sock.native_handle(), opt.so_mark_);
		if (ret.has_error())
			XLOG_WARN << "tun set socket mark: " << ret.error().message();
	}

	// 解析并连接上游代理服务器，成功返回已连接的 socket；失败返回未打开的 socket.
	net::awaitable<tcp::socket> connect_proxy_pass(
		net::any_io_executor executor,
		const proxy_server_option& opt,
		const urls::url& proxy_url,
		const std::function<net::awaitable<bool>(int)>& protect);

	// 发送 HTTP CONNECT 请求建立隧道，成功返回 true.
	template <typename Stream>
	net::awaitable<bool> http_connect_tunnel(Stream& stream,
		const std::string& authority, const urls::url& proxy_url)
	{
		boost::system::error_code ec;

		http::request<http::empty_body> req{
			http::verb::connect, authority, 11 };
		req.set(http::field::host, authority);
		req.set(http::field::user_agent, "xproxy/1.0");
		if (!proxy_url.user().empty())
		{
			const auto userinfo = std::string(proxy_url.user()) + ":" +
				std::string(proxy_url.password());
			req.set(http::field::proxy_authorization,
				"Basic " + strutil::base64_encode(userinfo));
		}

		co_await http::async_write(stream, req, net_awaitable[ec]);
		if (ec)
			co_return false;

		beast::flat_buffer buf;
		http::response_parser<http::empty_body> parser;
		parser.skip(true);
		co_await http::async_read_header(
			stream, buf, parser, net_awaitable[ec]);
		if (ec)
			co_return false;

		auto res = parser.release();
		if (res.result() != http::status::ok)
		{
			XLOG_WARN << "tun doh connect tunnel rejected: "
				<< static_cast<int>(res.result());
			co_return false;
		}
		co_return true;
	}

	// 发送 DoH POST 请求并读取响应，成功返回 true 并填充 output.
	template <typename Stream>
	net::awaitable<bool> doh_post(Stream& stream,
		const std::string& host, const std::string& path,
		const std::string& dns_query, std::string& output,
		const std::string& username = {},
		const std::string& password = {})
	{
		boost::system::error_code ec;

		http::request<http::string_body> req{
			http::verb::post, path, 11 };
		req.set(http::field::host, host);
		req.set(http::field::content_type, "application/dns-message");
		req.set(http::field::accept, "application/dns-message");
		if (!username.empty())
		{
			const auto userinfo = username + ":" + password;
			req.set(http::field::authorization,
				"Basic " + strutil::base64_encode(userinfo));
		}
		req.body() = dns_query;
		req.prepare_payload();

		co_await http::async_write(stream, req, net_awaitable[ec]);
		if (ec)
			co_return false;

		beast::flat_buffer buf;
		http::response<http::string_body> res;
		co_await http::async_read(stream, buf, res, net_awaitable[ec]);
		if (ec)
			co_return false;

		if (res.result() != http::status::ok)
		{
			XLOG_WARN << "tun doh response status: "
				<< res.result_int();
			co_return false;
		}
		output = std::move(res.body());
		co_return true;
	}

	//////////////////////////////////////////////////////////////////////////
	// DoH keep-alive 连接复用

	// 单个挂起的 DNS 查询请求（wire-format 查询与完成回调）.
	struct doh_request
	{
		std::string dns_query;
		std::function<void(const boost::system::error_code&, std::string)> handler;
	};

	// doh_connection 实现一条到 DoH 服务的持久连接（HTTP/1.1 keep-alive）.
	// 查询请求入队后由 pump 协程串行发送（同一连接同一时刻仅一个 in-flight
	// 请求），空闲超过 k_idle_timeout 后自动关闭，请求失败时重建连接重试一次.
	class doh_connection
		: public std::enable_shared_from_this<doh_connection>
	{
	public:
		doh_connection(net::any_io_executor executor,
			proxy_server_option opt,
			std::function<net::awaitable<bool>(int)> protect);
		~doh_connection();

		doh_connection(const doh_connection&) = delete;
		doh_connection& operator=(const doh_connection&) = delete;

		// 提交 DNS 查询（wire-format），完成后同步调用 handler.
		// 连接已关闭或队列满时立即以失败调用 handler.
		void submit(std::string dns_query,
			std::function<void(const boost::system::error_code&, std::string)> handler);

		// 当前排队 + 处理中的请求数（供连接池负载均衡）.
		size_t pending() const noexcept;

		// 关闭连接并唤醒所有等待者（失败）.
		void close();

		bool closed() const noexcept { return m_closed; }

	private:
		// 处理队列的 pump 协程：串行发送请求，空闲时等待新请求或超时关闭.
		net::awaitable<void> pump();

		// 建立到 DoH 服务的连接（直连或经代理 CONNECT 隧道），成功返回 true.
		net::awaitable<bool> connect();

		// 经代理 CONNECT 隧道转发到 DoH 服务（https 代理先建外层 TLS，
		// 再建 CONNECT 隧道，内层按需 TLS），成功返回 true.
		net::awaitable<bool> connect_via_proxy(tcp::socket sock);

		// 直连 DoH 服务（与 proxy_pass 同服务，或把 proxy_pass 当作 DoH
		// 服务器），成功返回 true.
		net::awaitable<bool> connect_direct(tcp::socket sock);

		// 惰性初始化并配置 SSL context（证书校验与 SNI 使用 verify_host）.
		net::awaitable<bool> setup_tls(const std::string& verify_host);

		// 对 m_stream 中的 SSL 流设置 SNI 并完成客户端握手.
		net::awaitable<bool> tls_handshake(const std::string& host);

		// 发送单个 DNS 查询并读取响应，成功返回 true 并填充 output.
		net::awaitable<bool> exchange(const std::string& dns_query, std::string& output);

		// 关闭底层连接（不唤醒等待者）.
		void teardown();

	private:
		// 空闲超时：队列空后无新请求则关闭连接释放资源.
		static constexpr std::chrono::seconds k_idle_timeout{ 60 };

		// 单次请求处理超时：超时后关闭连接使挂起的读写立即失败.
		static constexpr std::chrono::seconds k_request_timeout{ 15 };

		// 单连接请求队列上限，超出直接丢弃（客户端可重试）.
		static constexpr size_t k_max_pending = 128;

		net::any_io_executor m_executor;
		proxy_server_option m_option;
		std::function<net::awaitable<bool>(int)> m_protect;

		// DoH 目标（构造时解析）.
		std::string m_doh_host;
		std::string m_doh_path;
		uint16_t m_doh_port { 443 };
		bool m_doh_https { true };
		bool m_doh_via_proxy { false };
		std::string m_auth_user;
		std::string m_auth_pass;

		// 底层连接（直连 TLS/明文，或经代理 CONNECT 隧道后的流）.
		std::optional<variant_stream_type> m_stream;

		// 请求队列（单 io_context 线程访问，无需加锁）.
		std::deque<doh_request> m_queue;

		// 空闲/请求超时定时器.
		net::steady_timer m_idle_timer;
		net::steady_timer m_req_timer;

		// 直连/内层 TLS 使用的 SSL context（连接重建时复用）.
		std::optional<net::ssl::context> m_ssl_ctx;
		bool m_ssl_configured { false };

		bool m_closed { false };
		bool m_pumping { false };
		bool m_busy { false };
	};

	// doh_client 维护到 DoH 服务的连接池（最多 k_max_connections 条），
	// 供多个 tun_udp_flow 共享复用 keep-alive 连接.
	class doh_client
	{
	public:
		doh_client(net::any_io_executor executor,
			proxy_server_option opt,
			std::function<net::awaitable<bool>(int)> protect);

		// 提交 DNS 查询（wire-format），返回 wire-format 响应；
		// 失败返回空串（调用方按 SERVFAIL 处理）.
		net::awaitable<std::string> query(const std::string& dns_query);

		// 关闭所有连接并唤醒等待者.
		void close();

	private:
		// 选择一条连接：优先空闲，未达上限时新建，否则选排队最少者.
		std::shared_ptr<doh_connection> pick();

	private:
		// 连接池上限（并发连接数）.
		static constexpr size_t k_max_connections = 2;

		net::any_io_executor m_executor;
		proxy_server_option m_option;
		std::function<net::awaitable<bool>(int)> m_protect;

		std::vector<std::shared_ptr<doh_connection>> m_conns;
	};

} // namespace proxy

#endif // INCLUDE__2026_08_25__DOH_CLIENT_HPP
