//
// dns_server.hpp
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef INCLUDE__2026_08_10__DNS_SERVER_HPP
#define INCLUDE__2026_08_10__DNS_SERVER_HPP


#include "proxy/proxy_session.hpp"
#include "proxy/dns_response_cache.hpp"

#include <atomic>
#include <memory>


namespace proxy {

	// dns_server 实现 UDP DNS 服务器：监听 dns_udp_port_ 端口，把接收到的
	// DNS 查询转发到 dns_upstream_（若配置），否则按系统默认解析流程构造
	// 响应。支持 dns_cache_size_/dns_cache_ttl_ 配置的查询结果缓存与
	// dns_no_ipv6_（AAAA 查询返回空应答）。
	//
	// 生命周期由 proxy_server 管理：start() 启动监听，close() 停止；
	// apply_options() 支持运行期热改（端口/缓存/no_ipv6）。
	//
	// 线程要求：与 proxy_server 的其他协程一致，所有成员函数（start/close/
	// apply_options）都必须在运行于 m_executor 的 io_context 线程上调用
	// （launcher 控制通道协程即运行于该上下文）；内部状态访问无额外加锁，
	// 依赖单线程执行保证一致性.
	class dns_server
	{
		dns_server(const dns_server&) = delete;
		dns_server& operator=(const dns_server&) = delete;

	public:
		dns_server(
			net::any_io_executor executor,
			net::io_context& backend_context,
			bool scheduler_locking,
			proxy_server_option option);

		// 启动 UDP DNS 监听（若配置了 dns_udp_port_ > 0）.
		void start() noexcept;

		// 停止 UDP DNS 监听（关闭 socket 使监听协程退出）.
		void close() noexcept;

		// 运行期热改 DNS 相关选项（dns_udp_port_/dns_cache_size_/
		// dns_cache_ttl_/dns_no_ipv6_）。返回错误信息，空串表示成功.
		std::string apply_options(const proxy_server_option& opt);

		// 访问 DNS 查询结果缓存（未启用缓存返回 nullptr）.
		dns_response_cache* cache() noexcept;

		// 是否禁用 DNS 的 IPv6 解析返回（AAAA 查询返回空应答）.
		bool no_ipv6() const noexcept;

	private:
		// UDP DNS 监听协程，sock 由 shared_ptr 持有以保证协程退出前 socket
		// 不会因热改/关闭而被销毁（UAF 防护）.
		net::awaitable<void> udp_listen(std::shared_ptr<udp::socket> sock);

		// 处理单个 UDP DNS 查询并回包.
		net::awaitable<void> handle_query(
			const std::shared_ptr<udp::socket>& sock,
			const udp::endpoint& peer, std::string query);

		// 通过 UDP 上游转发 DNS 查询，成功返回 true.
		net::awaitable<bool> udp_query_raw(
			const std::string& dns_query, std::string& output);

		// 通过 DoH 上游转发 DNS 查询，成功返回 true.
		net::awaitable<bool> doh_query_raw(
			const std::string& dns_query, std::string& output);

		// 解析 DoH 上游主机地址.
		net::awaitable<tcp::resolver::results_type>
		resolve_host(const std::string& host, uint16_t port);

		// 按系统默认解析流程构造响应（无 dns_upstream 时使用）.
		net::awaitable<void> resolve_normal(
			const std::string& dns_query, std::string& output);

		// 根据当前配置重建缓存（size/ttl 变化时清空重建）.
		void rebuild_cache() noexcept;

		// 启动 UDP 监听（创建 socket 并 bind）.
		void start_listen() noexcept;

		// 停止 UDP 监听（关闭 socket）.
		void stop_listen() noexcept;

	private:
		net::any_io_executor m_executor;
		net::io_context& m_backend_context;
		bool m_scheduler_locking;
		proxy_server_option m_option;

		// 当前 UDP 监听 socket（共享指针，监听协程与查询协程共同持有）.
		std::shared_ptr<udp::socket> m_udp_socket;

		// DNS 查询结果缓存（未启用为 nullptr）.
		std::unique_ptr<dns_response_cache> m_cache;

		// 在途查询协程计数（防 UDP 洪泛导致协程无界增长）.
		std::atomic<int> m_inflight{ 0 };

		// 停止标志（close() 置位）.
		std::atomic_bool m_abort{ false };
	};

}

#endif // INCLUDE__2026_08_10__DNS_SERVER_HPP
