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
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>


namespace proxy {

	//////////////////////////////////////////////////////////////////////////
	// DNS wire-format 工具（命名空间级自由函数）。
	//
	// 供 UDP DNS 服务器（dns_server）与 HTTP DoH 路径（proxy_session）共享，
	// 原为 proxy_session 的 static 成员函数，与连接状态无关，故独立于此。

	// DNS 记录类型常量.
	inline constexpr uint16_t DNS_TYPE_A = 1;
	inline constexpr uint16_t DNS_TYPE_NS = 2;
	inline constexpr uint16_t DNS_TYPE_CNAME = 5;
	inline constexpr uint16_t DNS_TYPE_SOA = 6;
	inline constexpr uint16_t DNS_TYPE_PTR = 12;
	inline constexpr uint16_t DNS_TYPE_MX = 15;
	inline constexpr uint16_t DNS_TYPE_TXT = 16;
	inline constexpr uint16_t DNS_TYPE_AAAA = 28;
	inline constexpr uint16_t DNS_TYPE_SRV = 33;
	inline constexpr uint16_t DNS_TYPE_SVCB = 64;
	inline constexpr uint16_t DNS_TYPE_HTTPS = 65;
	inline constexpr uint16_t DNS_TYPE_ANY = 255;
	inline constexpr uint16_t DNS_TYPE_CAA = 257;
	inline constexpr uint16_t DNS_CLASS_IN = 1;

	// DNS 应答记录（用于构造 wire-format 响应报文）.
	struct dns_answer
	{
		std::string name;  // 所有者域名（完整，无压缩）.
		uint16_t type{ 1 }; // 记录类型（A/AAAA/CNAME...）.
		uint32_t ttl{ 60 }; // 存活时间（秒）.
		std::string data;  // RDATA（如 A 记录的 4 字节 IP）.
	};

	// dns_encode_name 将域名编码为 DNS wire-format 标签序列.
	std::string dns_encode_name(const std::string& name) noexcept;

	// dns_type_from_string 将类型名字符串转换为 DNS 类型数值.
	uint16_t dns_type_from_string(const std::string& type_name) noexcept;

	// dns_type_to_string 将 DNS 类型数值转换为字符串.
	std::string dns_type_to_string(uint16_t type) noexcept;

	// build_dns_wire_query 构建 DNS wire-format 查询包.
	std::string build_dns_wire_query(
		const std::string& name, uint16_t type,
		bool cd = false, bool do_bit = false) noexcept;

	// dns_parse_name 从 wire-format 解析域名 (支持压缩指针).
	std::pair<std::string, const char*>
	dns_parse_name(const char* p, const char* end, const char* msg_start) noexcept;

	// dns_svcparams_to_string 解析 HTTPS/SVCB 记录的 SvcParams (RFC 9460).
	std::string dns_svcparams_to_string(
		const char* p, const char* end) noexcept;

	// dns_rdata_to_string 将 RDATA 按类型解析为可读字符串.
	std::string dns_rdata_to_string(
		uint16_t type, uint16_t rdlength,
		const char* rdata, const char* end,
		const char* msg_start) noexcept;

	// dns_response_to_json 将 DNS wire-format 响应解析为 Google JSON API 格式.
	std::string dns_response_to_json(
		const std::string& wire_response,
		const std::string& question_name,
		uint16_t question_type) noexcept;

	// 从 wire-format 查询报文解析查询域名与类型，成功返回 true.
	bool dns_parse_query(
		const std::string& query, std::string& name, uint16_t& type) noexcept;

	// 提取查询报文中的 CD 与 DO 标志位.
	void dns_query_flags(
		const std::string& query, bool& cd, bool& do_bit) noexcept;

	// 生成 DNS 缓存键（域名 + 类型 + CD/DO 标志）.
	std::string dns_cache_key(
		const std::string& name, uint16_t type, bool cd, bool do_bit) noexcept;

	// 返回剥离事务 ID 的响应副本（缓存存储用）.
	std::string dns_strip_id(const std::string& resp) noexcept;

	// 返回写入指定事务 ID 的响应副本（缓存命中回包用）.
	std::string dns_set_id(const std::string& resp, uint16_t id) noexcept;

	// 判断响应是否可缓存（SERVFAIL 是临时故障，不缓存）.
	bool dns_cacheable(const std::string& resp) noexcept;

	// 根据查询报文构建 DNS wire-format 响应，回显问题并携带应答.
	// rcode 为响应码（0=NOERROR, 1=FORMERR, 2=SERVFAIL, 3=NXDOMAIN）.
	// 查询报文过短/无法解析时返回空串.
	std::string dns_build_response(
		const std::string& query, int rcode,
		const std::vector<dns_answer>& answers) noexcept;

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

		// 向 peer 回送 DNS 响应报文，成功返回 true；失败记录日志.
		net::awaitable<bool> send_response(
			const std::shared_ptr<udp::socket>& sock,
			const udp::endpoint& peer, const std::string& response);

		// 经配置的上游转发 DNS 查询：https 走 DoH，其余走 UDP.
		net::awaitable<bool> query_upstream(
			const std::string& dns_query, std::string& output);

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

		// 根据 m_option.local_ip_ 解析向外发起请求（UDP/DoH 上游）时的
		// 出口绑定地址；为空或解析失败时重置（由系统路由自动选择源地址）.
		void update_bind_interface() noexcept;

	private:
		net::any_io_executor m_executor;
		net::io_context& m_backend_context;
		bool m_scheduler_locking;
		proxy_server_option m_option;

		// 向外（UDP/DoH 上游）发起请求时绑定的本地源地址（来自 local_ip_）.
		std::optional<net::ip::address> m_bind_interface;

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
