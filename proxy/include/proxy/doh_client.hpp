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
#include <mutex>
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

	// proxy_pass_pool 前置声明（doh_connection 持有其指针复用已建立连接）.
	class proxy_pass_pool;

	// doh_connection 实现一条到 DoH 服务的持久连接（HTTP/1.1 keep-alive）.
	// 查询请求入队后由 pump 协程串行发送（同一连接同一时刻仅一个 in-flight
	// 请求），空闲超过 k_idle_timeout 后自动关闭，请求失败时重建连接重试一次.
	// 建连时优先从 proxy_pass_pool 取一条已建立（TCP/TLS 已完成）的连接复用.
	class doh_connection
		: public std::enable_shared_from_this<doh_connection>
	{
	public:
		doh_connection(net::any_io_executor executor,
			proxy_server_option opt,
			std::function<net::awaitable<bool>(int)> protect,
			std::shared_ptr<proxy_pass_pool> proxy_pool);
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
		// 优先从 proxy_pass_pool 获取已建立的连接，取不到再新建.
		net::awaitable<bool> connect();

		// 基于从连接池获取的已有连接（TCP/TLS 已完成）建立 DoH 连接，
		// 成功返回 true.
		net::awaitable<bool> connect_pooled(variant_stream_type stream);

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

		// proxy_pass 预选连接池（可空；建连时优先从中复用已建立的连接）.
		std::shared_ptr<proxy_pass_pool> m_proxy_pool;

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
			std::function<net::awaitable<bool>(int)> protect,
			std::shared_ptr<proxy_pass_pool> proxy_pool);

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

		// proxy_pass 预选连接池（传递到各 doh_connection 复用）.
		std::shared_ptr<proxy_pass_pool> m_proxy_pool;

		std::vector<std::shared_ptr<doh_connection>> m_conns;
	};

	//////////////////////////////////////////////////////////////////////////
	// proxy_pass 预选连接池

	// proxy_pass_pool 维护一组预先建立好的到 proxy_pass 的连接
	// （TCP 与可选 TLS 握手均已完成），供 tun_server 的 TCP flow 复用，
	// 避免每次新连接都重复 TCP 三次握手与 TLS 握手.
	//
	// - 启动后立即连续建连直到池满 m_target 条（预热不限速）；
	// - 连接被取走、意外/超时断开或保活重建后，立即异步补建维持池满；
	// - 建连失败按 k_retry_interval 间隔重试；
	// - 空闲连接超过 k_idle_timeout 未被使用会被整体重建（保活），
	//   防止代理端空闲超时断开导致复用失败.
	class proxy_pass_pool
		: public std::enable_shared_from_this<proxy_pass_pool>
	{
	public:
		proxy_pass_pool(net::any_io_executor executor,
			proxy_server_option opt,
			std::function<net::awaitable<bool>(int)> protect,
			size_t target);

		~proxy_pass_pool();

		proxy_pass_pool(const proxy_pass_pool&) = delete;
		proxy_pass_pool& operator=(const proxy_pass_pool&) = delete;

		// 启动维护协程（预建连接并保活）.
		void start();

		// 从池中取出一条已建立（TCP/TLS 已完成）的连接；
		// 池空时直接新建一条返回；失败返回 nullopt.
		net::awaitable<std::optional<variant_stream_type>> acquire();

		// 关闭池并释放所有空闲连接.
		void close();

		// 返回当前可用（空闲）连接数量.
		size_t idle_count() const noexcept;

		// 返回池目标连接数量.
		size_t target() const noexcept { return m_target; }

	private:
		// 维护协程：池未满立即连续补建（预热不限速），
		// 建连失败按 k_retry_interval 重试，空闲超时整体重建保活.
		net::awaitable<void> maintain();

		// 建立一条到 proxy_pass 的连接（TCP + 可选 TLS），
		// 成功返回已连接的流，失败返回未打开的流.
		net::awaitable<variant_stream_type> make_connection();

		// 建立并加入一条空闲连接，成功返回 true.
		net::awaitable<bool> build_one();

		// 健康检查：剔除已被对端断开的空闲连接.
		void prune_dead();

		// 判定一条空闲连接是否仍可用（对端未断开）.
		static bool is_conn_alive(variant_stream_type& stream) noexcept;

	private:
		// 建连失败后的重试间隔.
		static constexpr std::chrono::seconds k_retry_interval{ 5 };

		// 空闲连接保活超时：超过后整体重建，避免被代理端空闲断开.
		static constexpr std::chrono::seconds k_idle_timeout{ 60 };

		net::any_io_executor m_executor;
		proxy_server_option m_option;
		std::function<net::awaitable<bool>(int)> m_protect;

		// 维持的空闲连接目标数量.
		size_t m_target { 0 };

		// 标记池是否曾达到过目标数量（预热完成进入稳态）.
		bool m_reached_target { false };

		// 空闲连接池（variant_stream_type 为 move-only）.
		std::deque<variant_stream_type> m_idle;

		// 保护 m_idle 的并发访问（idle_count 为 const 方法故声明 mutable）.
		mutable std::mutex m_mutex;

		// SSL context（惰性初始化，供 TLS 连接复用）.
		std::optional<net::ssl::context> m_ssl_ctx;

		// 维护定时器与维护协程运行状态.
		std::optional<net::steady_timer> m_timer;
		bool m_maintaining { false };
		bool m_closed { false };
	};

} // namespace proxy

#endif // INCLUDE__2026_08_25__DOH_CLIENT_HPP
