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
#include "proxy/dns_response_cache.hpp"
#include "proxy/doh_client.hpp"

#include "tunio/tunio.hpp"
#include "tunio/tun_tcp_socket.hpp"
#include "tunio/tun_tcp_acceptor.hpp"
#include "tunio/tun_udp_socket.hpp"
#include "tunio/tun_udp_acceptor.hpp"

#include <chrono>
#include <deque>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace proxy {

	// 连接协议标识（供 launcher 连接明细展示）.
	enum class tun_conn_proto : uint8_t
	{
		tcp = 1,        // TCP 直连
		http,           // HTTP CONNECT 代理隧道
		https,          // HTTPS CONNECT 代理隧道
		socks5,         // SOCKS5（TCP 或 UDP ASSOCIATE）
		udp,            // UDP 直连
		connect_udp,    // RFC 9298 CONNECT-UDP 隧道
		doh_dns,        // DNS 查询经 DoH 转发到 proxy_pass
	};

	// tcp_flow_key 标识一个 TCP 连接或 UDP 会话（源/目的地址与端口）.
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

	// tun_server 前置声明（供 flow 访问写回接口）.
	class tun_server;

	// tun_tcp_flow 实现一个 TCP 连接的转发：
	// - tunio 引擎完成三次握手后经 async_accept 交付 tun_tcp_socket；
	// - 按分流规则经 proxy_pass 或直连建立上游连接；
	// - 双向搬运客户端（tun_tcp_socket）与上游之间的数据，处理半关闭。
	class tun_tcp_flow
		: public std::enable_shared_from_this<tun_tcp_flow>
	{
		friend class tun_server;

	public:
		tun_tcp_flow(net::any_io_executor executor,
			const std::shared_ptr<tun_server>& owner,
			const proxy_server_option& opt,
			tcp_flow_key key,
			tunio::tun_tcp_socket stream);

		~tun_tcp_flow();

		// 发起上游连接（由 tun_server 收到 accept 时调用）.
		void start();

		// 关闭 flow，释放上游连接与客户端流.
		void close();

		bool closed() const noexcept { return m_closed; }

	private:
		// 发起上游连接（代理握手或直连），成功后启动双向搬运协程.
		net::awaitable<void> do_connect();

		// 建立上游连接（代理握手或直连），成功返回 true.
		net::awaitable<bool> establish_upstream(
			const std::string& target_host, uint16_t target_port,
			bool use_proxy);

		// 经上游代理建立连接（含 TLS 与代理协议握手），成功返回 true.
		net::awaitable<bool> establish_via_proxy(
			const std::string& target_host, uint16_t target_port,
			const std::function<net::awaitable<bool>(int)>& protect);

		// 直连目标建立连接，成功返回 true.
		net::awaitable<bool> establish_direct(
			uint16_t target_port,
			const std::function<net::awaitable<bool>(int)>& protect);

		// 与上游代理完成协议握手（SOCKS5 或 HTTP CONNECT）.
		net::awaitable<bool> do_proxy_handshake(const urls::url& proxy_url);

		// 客户端（tun_tcp_socket）数据转发到上游的发送协程.
		net::awaitable<void> tx_loop();

		// 读取上游数据并转发到客户端的接收协程.
		net::awaitable<void> rx_loop();

		// 处理客户端 FIN：半关闭上游写方向，等待上游 EOF 后整体关闭.
		void handle_client_fin();

		// 处理上游读取结束：EOF 向客户端发 FIN，异常发送 RST 并关闭.
		void handle_upstream_eof(boost::system::error_code ec, size_t n);

		// 上游连接异常/关闭处理.
		void on_upstream_closed();

	private:
		// m_executor 保存当前 io_context 的 executor.
		net::any_io_executor m_executor;

		// m_owner 保存所属 tun_server（统计与 flow 管理）.
		std::shared_ptr<tun_server> m_owner;

		// m_option 保存服务器配置选项.
		proxy_server_option m_option;

		// m_key 保存本连接的标识.
		tcp_flow_key m_key;

		// m_conn_id 保存本连接在 launcher 连接明细中的唯一标识.
		uint64_t m_conn_id { 0 };

		// m_client/m_target 保存连接的四元组（供连接明细展示）.
		net::ip::tcp::endpoint m_client;
		net::ip::tcp::endpoint m_target;

		// m_started 记录连接创建时间（供连接明细计算已持续时长）.
		std::chrono::steady_clock::time_point m_started;

		// 本连接累计收发字节数（客户端 → 上游 / 上游 → 客户端）.
		std::atomic<uint64_t> m_rx_bytes { 0 };
		std::atomic<uint64_t> m_tx_bytes { 0 };

		// m_proto 保存协议标识（tun_conn_proto 值）.
		std::atomic<uint8_t> m_proto { 0 };

		// m_client_stream 保存 tunio 交付的客户端 TCP 流.
		tunio::tun_tcp_socket m_client_stream;

		// m_upstream 保存与上游代理或目标的连接.
		variant_stream_type m_upstream;

		// 连接状态.
		bool m_closed { false };
		bool m_connected { false };
		bool m_client_fin { false };
		bool m_upstream_eof { false };
	};

	//////////////////////////////////////////////////////////////////////////
	// UDP flow

	// tun_udp_flow 实现一个 UDP 会话的转发：
	// - tunio 引擎按客户端三元组建立会话，首数据报确定会话目标；
	// - 按分流规则经 SOCKS5 ASSOCIATE / CONNECT-UDP 隧道或直连发送；
	// - 从后端接收应答并经 tun_udp_socket 回写客户端；
	// - 无流量超时后自动关闭，释放后端连接。
	class tun_udp_flow
		: public std::enable_shared_from_this<tun_udp_flow>
	{
		friend class tun_server;

	public:
		tun_udp_flow(net::any_io_executor executor,
			const std::shared_ptr<tun_server>& owner,
			const proxy_server_option& opt,
			tunio::tun_udp_socket socket);

		~tun_udp_flow();

		// 发起后端连接并进入接收循环（由 tun_server 收到 accept 时调用）.
		void start();

		// 关闭会话，释放后端连接与客户端流.
		void close();

		bool closed() const noexcept { return m_closed; }

	private:
		// 接收首数据报确定会话目标，建立后端后进入接收循环.
		net::awaitable<void> do_open();

		// 处理一个客户端数据报（DNS 缓存/DoH/转发）.
		net::awaitable<void> handle_datagram(const char* data, size_t len);

		// 直连模式发送 UDP 数据到目标, 成功返回 true.
		bool send_direct(const char* data, size_t len);

		// SOCKS5 模式封装 UDP 头后发送到中继, 成功返回 true.
		bool send_via_socks5(const char* data, size_t len);

		// 判定该 UDP 流是否走上游代理.
		bool resolve_proxy_route(bool& self_query);

		// 对 DNS 查询按域名区分国内/国外分流.
		void resolve_dns_route(bool& use_proxy, bool self_query);

		// 判定 UDP 传输模式（CONNECT-UDP / SOCKS5 ASSOCIATE）.
		bool resolve_udp_transport();

		// 综合分流结果确定代理模式.
		bool resolve_proxy_mode();

		// 与上游代理建立 UDP 通道（CONNECT-UDP 或 SOCKS5 ASSOCIATE）.
		net::awaitable<bool> establish_proxy();

		// 打开本地 UDP 后端 socket（直连或 SOCKS5 模式使用）.
		net::awaitable<bool> open_backend();

		// 与上游代理完成 SOCKS5 UDP ASSOCIATE 握手.
		net::awaitable<bool> do_socks5_associate(tcp::socket sock);

		// 建立 HTTP CONNECT-UDP 隧道（proxy_pass 为 http/https 时替代 SOCKS5 ASSOCIATE）.
		net::awaitable<bool> do_connect_udp(tcp::socket sock);

		// 发送 RFC 9298 CONNECT-UDP 请求，成功返回 true.
		net::awaitable<bool> send_connect_udp_request();

		// 读取并校验 CONNECT-UDP 响应（须为 101），成功返回 true.
		net::awaitable<bool> read_connect_udp_response();

		// 启动串行发送协程与 capsule 接收循环.
		void start_data_loops();

		// 串行发送 CONNECT-UDP capsule 到 TCP 控制连接.
		net::awaitable<void> tx_loop();

		// 从 TCP 控制连接接收 capsule 并回写客户端.
		net::awaitable<void> recv_connect_udp_loop();

		// 将客户端 UDP 数据封装为 DATAGRAM capsule 并入队.
		void push_capsule(const char* data, size_t len);

		// 从控制流读取一个完整 capsule（type + value）.
		net::awaitable<std::pair<uint64_t, std::vector<char>>>
		read_capsule(boost::system::error_code& ec);

		// 保持 SOCKS5 ASSOCIATE 控制连接存活（读取直到断开）.
		net::awaitable<void> control_loop();

		// 接收后端应答并回写客户端.
		net::awaitable<void> recv_loop();

		// 向客户端回包（源地址为会话目标 m_target）.
		net::awaitable<void> reply(
			const char* data, size_t len, bool cache_resp = true);

		// 将 DNS 响应写入查询结果缓存（从响应报文解析键）.
		void cache_dns_response(const char* data, size_t len) noexcept;

		// 重置过期计时器.
		void touch();

	private:
		// m_executor 保存当前 io_context 的 executor.
		net::any_io_executor m_executor;

		// m_owner 保存所属 tun_server（统计与 flow 管理）.
		std::shared_ptr<tun_server> m_owner;

		// m_option 保存服务器配置选项.
		proxy_server_option m_option;

		// m_key 保存本会话的标识.
		tcp_flow_key m_key;

		// m_client 保存 TUN 侧客户端地址.
		net::ip::udp::endpoint m_client;

		// m_target 保存会话目标地址（DNS 分流可能改写为实际转发目标）.
		net::ip::udp::endpoint m_target;

		// m_orig_target 保存客户端请求的原始目标（首数据报远端），
		// 用于识别会话目标变化（tunio 会话按客户端三元组复用）.
		net::ip::udp::endpoint m_orig_target;

		// m_dns_qname 保存 DNS 查询域名（目标 53 端口时），用于按域名分流.
		std::string m_dns_qname;

		// m_conn_id 保存本会话在 launcher 连接明细中的唯一标识.
		uint64_t m_conn_id { 0 };

		// m_started 记录会话创建时间（供连接明细计算已持续时长）.
		std::chrono::steady_clock::time_point m_started;

		// 本会话累计收发字节数（客户端 → 后端 / 后端 → 客户端）.
		std::atomic<uint64_t> m_rx_bytes { 0 };
		std::atomic<uint64_t> m_tx_bytes { 0 };

		// m_proto 保存协议标识（tun_conn_proto 值）.
		std::atomic<uint8_t> m_proto { 0 };

		// m_connect_udp 标记 HTTP CONNECT-UDP 模式（proxy_pass 为 http 时）.
		bool m_connect_udp { false };

		// m_doh_mode 标记 DNS 查询以 DoH 直连 proxy_pass（同服务）的模式.
		bool m_doh_mode { false };

		// m_doh_via_proxy 标记 DNS 查询以 DoH 经 proxy_pass CONNECT 隧道转发.
		bool m_doh_via_proxy { false };

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

		// m_send_endp 保存 DNS 查询的实际发送目标（直连模式）.
		// self_query（proxy 自身解析 proxy_pass 域名）时为避免直连被污染的
		// 注入 DNS（8.8.8.8 等），改发国内 DNS；响应回包源地址仍用 m_target
		// （客户端 socket 与原始 DNS 建立连接，源地址必须与之匹配）.
		net::ip::udp::endpoint m_send_endp;

		// m_proxy 标记是否走上游代理.
		bool m_proxy { false };

		// m_data_loops_started 标记 CONNECT-UDP 收发协程已启动
		// （do_connect_udp 与 do_open 均可能触发, 防止重复启动）.
		bool m_data_loops_started { false };

		// m_ready 标记后端已就绪（do_open 完成后置位）.
		bool m_ready { false };

		// m_pending 保存后端就绪前到达的客户端数据，就绪后补发.
		std::deque<std::string> m_pending;

		// 待发送数据（m_pending/m_tx_queue）上限，防止后端建连缓慢时
		// 内存无界增长.
		static constexpr size_t k_max_udp_pending = 1024;

		// m_closed 标记是否已关闭.
		bool m_closed { false };

		// m_client_socket 保存 tunio 交付的客户端 UDP 会话.
		tunio::tun_udp_socket m_client_socket;

		// m_expire 无流量超时定时器.
		std::optional<net::steady_timer> m_expire;
	};

	//////////////////////////////////////////////////////////////////////////
	// tun_server

#if defined(__linux__) || defined(__APPLE__) || defined(_WIN32)

	// tun_server 实现 TUN2SOCKS 服务：基于 tunio 引擎从 TUN 设备捕获
	// TCP/UDP 流量，按分流规则（proxy_domains_/proxy_cidr_）经 proxy_pass_
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

		// 打开 TUN 设备并启动 accept 循环.
		// - tun_wait_fd_ 为 true 时不创建设备，等待 set_tun_fd 注入后启动.
		// - 否则按 tun_fd_（外部注入）或 tun_name_（自建设备）打开.
		void start() noexcept;

		// 停止 accept 循环并关闭引擎.
		void close() noexcept;

		// 注入外部 TUN fd（Android VpnService 建立后 detach 的 fd）.
		// 替换旧设备并重启 accept 循环（若尚未启动）；须在 io_context 线程调用.
		void set_tun_fd(int fd) noexcept;

		// 设置出站 socket 的 protect 请求回调（Android VpnService 场景）.
		// 回调在 io_context 线程执行；为空表示无需 protect（放行）.
		void set_protect_handler(
			std::function<net::awaitable<bool>(int)> handler);

		// 设置 DNS 查询结果缓存（TUN 内 DNS 查询命中直接回包；
		// 未启用缓存时为 nullptr）.
		void set_dns_cache(dns_response_cache* cache) noexcept;

		// 返回当前 DNS 查询结果缓存（未启用为 nullptr）.
		dns_response_cache* dns_cache() const noexcept;

		// 请求对出站 socket fd 执行 protect（经回调）, 无回调或失败时放行.
		net::awaitable<bool> protect_socket(int fd);

		// 单个方向 (TCP/UDP) 并发 flow 数量上限. 超过时拒绝新建 flow,
		// 防止异常场景下流量风暴耗尽进程 fd.
		static constexpr size_t k_max_tcp_flows = 4096;
		static constexpr size_t k_max_udp_flows = 4096;

		// 流量与连接统计快照（供 launcher 状态上报合并）.
		struct stats
		{
			uint64_t rx_bytes { 0 };
			uint64_t tx_bytes { 0 };
			uint64_t conn_total { 0 };
			size_t active_connections { 0 };
			size_t pool_connections { 0 };
			size_t pool_target { 0 };
		};

		// 返回当前流量与连接统计.
		stats get_stats() noexcept;

		// 单条活跃连接明细（供 launcher 状态上报的连接列表）.
		struct conn_info
		{
			uint64_t id { 0 };
			std::string client_ip;
			std::string target;
			std::string proto;
			int64_t elapsed { 0 };
			uint64_t rx_bytes { 0 };
			uint64_t tx_bytes { 0 };
		};

		// 返回当前活跃连接的明细快照（供 launcher 状态上报合并）.
		std::vector<conn_info> connections() noexcept;

	private:
		// 打开 tunio 引擎（自建设备或外部句柄注入），成功返回 true.
		bool open_engine() noexcept;

		// 启动 TCP/UDP accept 循环.
		void start_accept_loops() noexcept;

		// TCP accept 循环：接受新连接并创建 tun_tcp_flow.
		net::awaitable<void> run_accept_tcp();

		// UDP accept 循环：接受新会话并创建 tun_udp_flow.
		net::awaitable<void> run_accept_udp();

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

		// 注册 UDP 会话（首数据报解析目标后调用）.
		void register_udp_flow(const tcp_flow_key& key,
			const std::shared_ptr<tun_udp_flow>& flow);

		// 移除并关闭 TCP flow（由 flow 自身或 close 调用）.
		void remove_tcp_flow(const tcp_flow_key& key,
			const tun_tcp_flow* flow);

		// 移除并关闭 UDP flow（由 flow 自身或 close 调用）.
		void remove_udp_flow(const tcp_flow_key& key,
			const tun_udp_flow* flow);

		// 关闭全部活动 flow（锁外逐个关闭，避免析构回调再次加锁）.
		void close_flows() noexcept;

		// 分配连接明细用的唯一标识.
		uint64_t next_conn_id() noexcept;

		// 累计一条非 DNS 会话（m_conn_total++）.
		void on_conn_created() noexcept { ++m_conn_total; }

		// 累计隧道收发流量（由 flow 转发数据时同步累加）.
		void add_traffic(uint64_t rx, uint64_t tx) noexcept
		{
			m_rx_bytes += rx;
			m_tx_bytes += tx;
		}

		// 返回 DoH 连接池（惰性创建；DNS 查询走 keep-alive 复用）.
		// 仅在 DoH 模式（proxy_pass_ 非空）下调用.
		doh_client* doh_pool()
		{
			if (!m_doh_client)
			{
				// DoH 建连复用 proxy_pass 预选连接池（若启用）.
				std::shared_ptr<proxy_pass_pool> proxy_pool;
				if (m_option.proxy_pass_pool_size_ > 0)
					proxy_pool = proxy_conn_pool();
				m_doh_client = std::make_unique<doh_client>(
					m_executor, m_option,
					[this](int fd) -> net::awaitable<bool>
					{ return protect_socket(fd); },
					std::move(proxy_pool));
			}
			return m_doh_client.get();
		}

		// 返回 proxy_pass 预选连接池（惰性创建；TCP flow 走代理时复用）.
		// 仅在 TUN 模式且 proxy_pass_ 非空时调用.
		std::shared_ptr<proxy_pass_pool> proxy_conn_pool()
		{
			if (!m_proxy_pool)
			{
				m_proxy_pool = std::make_shared<proxy_pass_pool>(
					m_executor, m_option,
					[this](int fd) -> net::awaitable<bool>
					{ return protect_socket(fd); },
					m_option.proxy_pass_pool_size_);
				m_proxy_pool->start();
			}
			return m_proxy_pool;
		}

	private:
		// m_ioc 保存引擎所属的 io_context（tunio 构造需要）.
		net::io_context& m_ioc;

		// m_executor 保存当前 io_context 的 executor.
		net::any_io_executor m_executor;

		// m_option 保存服务器配置选项.
		proxy_server_option m_option;

		// m_dns_cache 保存 DNS 查询结果缓存（由 proxy_server 注入，
		// 热改重建后更新；未启用为 nullptr）.
		dns_response_cache* m_dns_cache { nullptr };

		// m_doh_client 保存 DoH 连接池（DNS 查询 keep-alive 复用，
		// 惰性创建，tun_server 关闭时一并关闭）.
		std::unique_ptr<doh_client> m_doh_client;

		// m_proxy_pool 保存 proxy_pass 预选连接池（TCP flow 走代理时
		// 复用已建立的连接，惰性创建，tun_server 关闭时一并关闭）.
		std::shared_ptr<proxy_pass_pool> m_proxy_pool;

		// m_tunio 保存用户态 TUN 网络引擎（tunio 第三方库）.
		std::unique_ptr<tunio::tunio> m_tunio;

		// m_tcp_acceptor/m_udp_acceptor 保存引擎的 accept 接口.
		std::unique_ptr<tunio::tun_tcp_acceptor> m_tcp_acceptor;
		std::unique_ptr<tunio::tun_udp_acceptor> m_udp_acceptor;

		// m_protect_handler 保存出站 socket 的 protect 请求回调.
		std::function<net::awaitable<bool>(int)> m_protect_handler;

		// m_running 标记 accept 循环是否在运行（io_context 线程访问）.
		bool m_running { false };

		// m_abort 停止标志.
		std::atomic<bool> m_abort { false };

		// m_tcp_flows 保存当前所有 TCP 连接.
		std::unordered_map<tcp_flow_key, std::shared_ptr<tun_tcp_flow>,
			tcp_flow_key_hash> m_tcp_flows;

		// m_udp_flows 保存当前所有 UDP 会话.
		std::unordered_map<tcp_flow_key, std::shared_ptr<tun_udp_flow>,
			tcp_flow_key_hash> m_udp_flows;

		// 保护 m_tcp_flows/m_udp_flows 的并发访问.
		std::mutex m_flows_mutex;

		// 流量统计（flow 累计, 原子计数, 多 flow 并发累加）.
		std::atomic<uint64_t> m_rx_bytes { 0 };
		std::atomic<uint64_t> m_tx_bytes { 0 };
		std::atomic<uint64_t> m_conn_total { 0 };

		// m_conn_seq 连接明细标识分配器.
		std::atomic<uint64_t> m_conn_seq { 0 };

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
		void set_dns_cache(dns_response_cache*) noexcept {}

	private:
		net::any_io_executor m_executor;
		proxy_server_option m_option;
	};

#endif // defined(__linux__) || defined(__APPLE__) || defined(_WIN32)

} // namespace proxy

#endif // INCLUDE__2026_08_22__TUN_SERVER_HPP
