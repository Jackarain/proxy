//
// tun_server.hpp
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef INCLUDE__2026_08_22__TUN_SERVER_HPP
#define INCLUDE__2026_08_22__TUN_SERVER_HPP

#include "proxy/proxy_session.hpp"
#include "proxy/tun_device.hpp"

#include <memory>

#if defined(__linux__)
# include <boost/asio/posix/stream_descriptor.hpp>
#endif

namespace proxy {

	//////////////////////////////////////////////////////////////////////////
	// IP 包解析

	// IP 协议号.
	inline constexpr uint8_t ip_proto_tcp = 6;
	inline constexpr uint8_t ip_proto_udp = 17;

	// TCP 标志位.
	inline constexpr uint8_t tcp_flag_fin = 0x01;
	inline constexpr uint8_t tcp_flag_syn = 0x02;
	inline constexpr uint8_t tcp_flag_rst = 0x04;
	inline constexpr uint8_t tcp_flag_psh = 0x08;
	inline constexpr uint8_t tcp_flag_ack = 0x10;

	// ip_packet 保存从 TUN 设备读取的 IP 包解析结果.
	struct ip_packet
	{
		net::ip::address src;
		net::ip::address dst;
		uint8_t proto { 0 };
		uint16_t src_port { 0 };
		uint16_t dst_port { 0 };

		// TCP 字段.
		uint32_t seq { 0 };
		uint32_t ack { 0 };
		uint8_t flags { 0 };
		uint16_t tcp_hdr_len { 0 };

		// 载荷与完整包数据.
		const char* payload { nullptr };
		size_t payload_len { 0 };
		const char* raw { nullptr };
		size_t raw_len { 0 };
	};

	// parse_ip_packet 解析 IP 包（IPv4/IPv6，支持 TCP/UDP）.
	// 成功返回 true；分片包或未知协议返回 false（调用方丢弃）.
	bool parse_ip_packet(const char* data, size_t len, ip_packet& pkt) noexcept;

	// build_ip_packet 从 IP 层信息构造完整 IP 数据包，用于写回 TUN 设备.
	// 目前仅支持 IPv4，返回空串表示失败.
	std::string build_ip_packet(
		const net::ip::address& src, const net::ip::address& dst,
		uint8_t proto, const std::string& payload) noexcept;

	//////////////////////////////////////////////////////////////////////////
	// tun_server

#if defined(__linux__)

	// tun_server 实现 TUN2SOCKS 服务：从 TUN 设备读取 IP 数据包，解析
	// TCP/UDP 后按分流规则（proxy_domains_/proxy_cidr_）经 proxy_pass_
	// 转发到上游代理，未命中则直连目标.
	class tun_server
		: public std::enable_shared_from_this<tun_server>
	{
		tun_server(const tun_server&) = delete;
		tun_server& operator=(const tun_server&) = delete;

		tun_server(net::any_io_executor executor, proxy_server_option opt);

	public:
		static std::shared_ptr<tun_server>
		make(net::any_io_executor executor, proxy_server_option opt);

		~tun_server();

		// 打开 TUN 设备并启动读包循环.
		void start() noexcept;

		// 停止读包循环并关闭设备.
		void close() noexcept;

	private:
		// 读包循环协程.
		net::awaitable<void> run();

		// 处理一个 IP 数据包（解析后分发到 TCP/UDP 处理）.
		void handle_packet(const char* data, size_t len) noexcept;

		// 判断目标地址是否命中 proxy_cidr_ 代理表.
		bool cidr_match(const net::ip::address& addr) const noexcept;

		// 判断域名是否命中 proxy_domains_ 代理表（后缀匹配）.
		bool domain_match(const std::string& domain) const noexcept;

		// 处理 TCP 包（由 run 协程调用，内部再派生子协程）.
		void handle_tcp_packet(ip_packet& pkt) noexcept;

		// 处理 UDP 包（由 run 协程调用，内部再派生子协程）.
		void handle_udp_packet(ip_packet& pkt) noexcept;

	private:
		// m_executor 保存当前 io_context 的 executor.
		net::any_io_executor m_executor;

		// m_option 保存服务器配置选项.
		proxy_server_option m_option;

		// m_tun 保存 TUN 设备对象.
		std::unique_ptr<tun_device> m_tun;

		// m_stream 封装 TUN 设备 fd 的异步读写.
		std::optional<net::posix::stream_descriptor> m_stream;

		// m_abort 停止标志.
		bool m_abort { false };
	};

#else // !defined(__linux__)

	// 非 Linux 平台的空实现.
	class tun_server
	{
	public:
		static std::shared_ptr<tun_server>
		make(net::any_io_executor executor, proxy_server_option opt);

		~tun_server() = default;

		void start() noexcept {}
		void close() noexcept {}

	private:
		net::any_io_executor m_executor;
		proxy_server_option m_option;
	};

#endif // defined(__linux__)

} // namespace proxy

#endif // INCLUDE__2026_08_22__TUN_SERVER_HPP
