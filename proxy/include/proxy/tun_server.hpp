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
#include "proxy/proxy_stream.hpp"
#include "proxy/tun_device.hpp"

#include <deque>
#include <memory>
#include <mutex>
#include <tuple>

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
	// TCP flow

	// tcp_flow_key 标识一个 TCP 连接（源/目的地址与端口）.
	struct tcp_flow_key
	{
		net::ip::address src;
		uint16_t src_port { 0 };
		net::ip::address dst;
		uint16_t dst_port { 0 };

		bool operator==(const tcp_flow_key&) const = default;
	};

	// tcp_flow_key 的哈希函数.
	struct tcp_flow_key_hash
	{
		size_t operator()(const tcp_flow_key& k) const noexcept
		{
			size_t h = std::hash<net::ip::address>{}(k.src);
			boost::hash_combine(h, k.src_port);
			boost::hash_combine(h, std::hash<net::ip::address>{}(k.dst));
			boost::hash_combine(h, k.dst_port);
			return h;
		}
	};

	// tun_server 前置声明（供 tun_tcp_flow 访问写回接口）.
	class tun_server;

	// tun_tcp_flow 实现一个 TCP 连接的状态机与转发：
	// - 收到客户端 SYN 后按分流规则经 proxy_pass 或直连建立上游连接；
	// - 维护客户端/上游两侧的序号映射，payload 双向透传；
	// - 生成 ACK/SYN-ACK/FIN/RST 应答客户端。
	class tun_tcp_flow
		: public std::enable_shared_from_this<tun_tcp_flow>
	{
		friend class tun_server;

	public:
		tun_tcp_flow(net::any_io_executor executor,
			const std::shared_ptr<tun_server>& owner,
			const proxy_server_option& opt,
			tcp_flow_key key,
			const ip_packet& syn);

		~tun_tcp_flow();

		// 发起上游连接（由 tun_server 收到 SYN 时调用）.
		void start();

		// 处理来自客户端的 TCP 包.
		void handle_packet(const ip_packet& pkt);

		// 关闭 flow，释放上游连接.
		void close();

		bool closed() const noexcept { return m_closed; }

	private:
		// 发起上游连接（代理握手或直连），成功后回 SYN-ACK.
		net::awaitable<void> do_connect();

		// 建立上游连接（代理握手或直连），成功返回 true.
		net::awaitable<bool> establish_upstream(
			const std::string& target_host, uint16_t target_port,
			bool use_proxy);

		// 回 SYN-ACK 并启动双向数据搬运协程.
		void start_data_plane();

		// 与上游代理完成协议握手（SOCKS5 或 HTTP CONNECT）.
		net::awaitable<bool> do_proxy_handshake(const urls::url& proxy_url);

		// 处理客户端数据段（序号检查后透传上游）.
		void handle_data(const ip_packet& pkt);

		// 客户端数据转发到上游的发送协程.
		net::awaitable<void> tx_loop();

		// 读取上游数据并转发到客户端的接收协程.
		net::awaitable<void> rx_loop();

		// 构造 TCP 段并写回 TUN 设备.
		void send_tcp(uint32_t seq, uint32_t ack, uint8_t flags,
			const char* payload, size_t payload_len,
			bool with_mss = false, uint16_t mss = 0);

		// 向客户端回 ACK 包.
		void send_ack();

		// 向客户端发送 FIN 包.
		void send_fin();

		// 向客户端发送 RST 包.
		void send_rst();

		// 客户端数据入队并唤醒发送协程.
		void push_tx(std::string data);

		// 上游连接异常/关闭处理.
		void on_upstream_closed();

	private:
		// m_executor 保存当前 io_context 的 executor.
		net::any_io_executor m_executor;

		// m_owner 保存所属 tun_server（写回 TUN 设备）.
		std::shared_ptr<tun_server> m_owner;

		// m_option 保存服务器配置选项.
		proxy_server_option m_option;

		// m_key 保存本连接的标识.
		tcp_flow_key m_key;

		// m_upstream 保存与上游代理或目标的连接.
		variant_stream_type m_upstream;

		// 连接状态.
		bool m_closed { false };
		bool m_connected { false };
		bool m_client_fin { false };

		// 序号状态.
		uint32_t m_client_isn { 0 };
		uint32_t m_server_isn { 0 };
		uint32_t m_client_next_seq { 0 };
		uint32_t m_client_ack_seq { 0 };
		uint32_t m_server_next_seq { 0 };

		// 客户端已发送 FIN（发送协程清空队列后需半关闭上游）.
		bool m_tx_fin { false };

		// 上游已关闭（等待客户端 FIN 完成四路挥手）.
		bool m_upstream_eof { false };

		// 客户端到上游的发送队列.
		std::deque<std::string> m_tx_queue;
		std::mutex m_tx_mutex;
		std::optional<net::steady_timer> m_tx_signal;
	};

	//////////////////////////////////////////////////////////////////////////
	// UDP flow

	// tun_udp_flow 实现一个 UDP 会话的转发：
	// - 收到客户端 UDP 包后按分流规则经 SOCKS5 ASSOCIATE 或直连发送；
	// - 从后端接收应答并封装为 IP 包写回 TUN 设备；
	// - 无流量超时后自动关闭，释放后端连接。
	class tun_udp_flow
		: public std::enable_shared_from_this<tun_udp_flow>
	{
		friend class tun_server;

	public:
		tun_udp_flow(net::any_io_executor executor,
			const std::shared_ptr<tun_server>& owner,
			const proxy_server_option& opt,
			const tcp_flow_key& key,
			const net::ip::udp::endpoint& client,
			const net::ip::udp::endpoint& target,
			const std::string& dns_qname);

		~tun_udp_flow();

		// 发起后端连接（SOCKS5 ASSOCIATE 或直连）并启动接收循环.
		void start();

		// 转发客户端数据到后端.
		void send(const char* data, size_t len) noexcept;

		// 关闭 flow，释放后端连接.
		void close();

	private:
		// 建立后端连接（代理或直连）.
		net::awaitable<void> do_open();

		// 判定代理模式（分流结果与 CONNECT-UDP/SOCKS5 选择），不支持时返回 false.
		bool resolve_proxy_mode();

		// 建立与上游代理的连接并完成 UDP 协议握手.
		net::awaitable<bool> establish_proxy();

		// 初始化 CONNECT-UDP 控制流（TCP 或 SSL）.
		net::awaitable<bool> make_control_stream(tcp::socket sock);

		// 打开本地 UDP 后端 socket（直连或 SOCKS5 模式使用）.
		net::awaitable<bool> open_backend();

		// 与上游代理完成 SOCKS5 UDP ASSOCIATE 握手.
		net::awaitable<bool> do_socks5_associate(tcp::socket sock);

		// 建立 HTTP CONNECT-UDP 隧道（proxy_pass 为 http/https 时替代 SOCKS5 ASSOCIATE）.
		// sock 为已连接到上游代理的 TCP socket，由 do_open 传入.
		net::awaitable<bool> do_connect_udp(tcp::socket sock);

		// 串行发送 CONNECT-UDP capsule 到 TCP 控制连接.
		net::awaitable<void> tx_loop();

		// 从 TCP 控制连接接收 capsule 并回写客户端.
		net::awaitable<void> recv_connect_udp_loop();

		// 将客户端 UDP 数据封装为 DATAGRAM capsule 并入队.
		void push_capsule(const char* data, size_t len);

		// 从控制流读取一个完整 capsule（type + value）.
		net::awaitable<std::pair<uint64_t, std::vector<char>>>
		read_capsule(boost::system::error_code& ec);

		// 直连模式发送 UDP 数据到目标, 成功返回 true.
		bool send_direct(const char* data, size_t len);

		// SOCKS5 模式封装 UDP 头后发送到中继, 成功返回 true.
		bool send_via_socks5(const char* data, size_t len);

		// 保持 SOCKS5 ASSOCIATE 控制连接存活（读取直到断开）.
		net::awaitable<void> control_loop();

		// 接收后端应答并写回 TUN 设备.
		net::awaitable<void> recv_loop();

		// 向客户端回包（封装 IP/UDP 后写回 TUN）.
		void reply(const char* data, size_t len);

		// 重置过期计时器.
		void touch();

	private:
		// m_executor 保存当前 io_context 的 executor.
		net::any_io_executor m_executor;

		// m_owner 保存所属 tun_server（写回 TUN 设备）.
		std::shared_ptr<tun_server> m_owner;

		// m_option 保存服务器配置选项.
		proxy_server_option m_option;

		// m_key 保存本会话的标识.
		tcp_flow_key m_key;

		// m_client 保存 TUN 侧客户端地址.
		net::ip::udp::endpoint m_client;

		// m_target 保存客户端请求的目标地址.
		net::ip::udp::endpoint m_target;

		// m_dns_qname 保存 DNS 查询域名（目标 53 端口时），用于按域名分流.
		std::string m_dns_qname;

		// m_connect_udp 标记 HTTP CONNECT-UDP 模式（proxy_pass 为 http 时）.
		bool m_connect_udp { false };

		// m_tx_queue 保存待发送的 CONNECT-UDP capsule（串行写 TCP 控制连接）.
		std::mutex m_tx_mutex;
		std::deque<std::vector<char>> m_tx_queue;

		// m_tx_signal 通知发送协程有新数据到达.
		std::optional<net::steady_timer> m_tx_signal;

		// m_backend 保存与上游代理或目标的 UDP 连接.
		std::optional<net::ip::udp::socket> m_backend;

		// m_control 保存 SOCKS5 ASSOCIATE 控制连接（代理模式）.
		std::optional<variant_stream_type> m_control;

		// m_backend_endp 保存代理返回的 UDP 中继地址.
		net::ip::udp::endpoint m_backend_endp;

		// m_proxy 标记是否走上游代理.
		bool m_proxy { false };

		// m_ready 标记后端已就绪（do_open 完成后置位）.
		bool m_ready { false };

		// m_closed 标记是否已关闭.
		bool m_closed { false };

		// m_pending 保存后端就绪前到达的客户端数据，就绪后补发.
		std::deque<std::string> m_pending;

		// m_expire 无流量超时定时器.
		std::optional<net::steady_timer> m_expire;
	};

	//////////////////////////////////////////////////////////////////////////
	// tun_server

#if defined(__linux__) || defined(__APPLE__) || defined(_WIN32)

	// tun_server 实现 TUN2SOCKS 服务：从 TUN 设备读取 IP 数据包，解析
	// TCP/UDP 后按分流规则（proxy_domains_/proxy_cidr_）经 proxy_pass_
	// 转发到上游代理，未命中则直连目标.
	class tun_server
		: public std::enable_shared_from_this<tun_server>
	{
		friend class tun_tcp_flow;
		friend class tun_udp_flow;
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

		// 判断目标地址是否命中代理表（proxy_cidr_ 或域名解析缓存）.
		bool ip_match_proxy(const net::ip::address& addr) const noexcept;

		// 记录 DNS 响应中的 A/AAAA 解析结果（仅命中 proxy_domains_ 时生效）.
		void record_dns_answer(const char* data, size_t len) noexcept;

		// 建立与上游代理的 SSL 连接（按需初始化客户端 SSL context）.
		net::awaitable<boost::system::result<bool>>
		make_ssl_socket(tcp::socket& remote_socket,
			std::string_view sni, std::optional<variant_stream_type>& ssl_sock);

		// 处理 TCP 包（由 run 协程调用，内部再派生子协程）.
		void handle_tcp_packet(ip_packet& pkt) noexcept;

		// 处理 UDP 包（由 run 协程调用，内部再派生子协程）.
		void handle_udp_packet(ip_packet& pkt) noexcept;

		// 写 IP 包到 TUN 设备（串行化，多 flow 并发安全）.
		void write_packet(std::string packet);

		// 依次写出写队列中的 IP 包（写完成回调中链式调用）.
		void do_write();

		// 移除并关闭 TCP flow（由 flow 自身或 close 调用）.
		void remove_tcp_flow(const tcp_flow_key& key);

		// 移除并关闭 UDP flow（由 flow 自身或 close 调用）.
		void remove_udp_flow(const tcp_flow_key& key);

	private:
		// m_executor 保存当前 io_context 的 executor.
		net::any_io_executor m_executor;

		// m_option 保存服务器配置选项.
		proxy_server_option m_option;

		// m_tun 保存 TUN 设备对象.
		std::unique_ptr<tun_device> m_tun;

		// m_tcp_flows 保存当前所有 TCP 连接.
		std::unordered_map<tcp_flow_key, std::shared_ptr<tun_tcp_flow>,
			tcp_flow_key_hash> m_tcp_flows;

		// 保护 m_tcp_flows 的并发访问.
		std::mutex m_flows_mutex;

		// TUN 设备写队列（多 flow 串行写）.
		std::deque<std::string> m_write_queue;
		std::mutex m_write_mutex;
		bool m_writing { false };

		// m_udp_flows 保存当前所有 UDP 会话.
		std::unordered_map<tcp_flow_key, std::shared_ptr<tun_udp_flow>,
			tcp_flow_key_hash> m_udp_flows;

		// dns_ip_entry 保存域名解析缓存条目（IP 与过期时间）.
		struct dns_ip_entry
		{
			net::ip::address ip;
			std::chrono::steady_clock::time_point expire;
		};

		// m_ssl_client_context 保存与上游代理建立 SSL 连接使用的客户端 context.
		std::optional<net::ssl::context> m_ssl_client_context;

		// m_domain_ips 保存命中 proxy_domains_ 的域名解析结果（域名 -> IP 列表）.
		// 声明为 mutable 以便 ip_match_proxy 等 const 方法惰性清理过期条目.
		mutable std::unordered_map<std::string, std::vector<dns_ip_entry>> m_domain_ips;

		// 保护 m_domain_ips 的并发访问.
		mutable std::mutex m_domain_ips_mutex;

		// m_abort 停止标志.
		bool m_abort { false };
	};

#else // 不支持的平台

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

#endif // defined(__linux__) || defined(__APPLE__) || defined(_WIN32)

} // namespace proxy

#endif // INCLUDE__2026_08_22__TUN_SERVER_HPP
