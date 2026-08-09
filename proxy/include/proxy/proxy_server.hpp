//
// proxy_server.hpp
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef INCLUDE__2023_10_18__PROXY_SERVER_HPP
#define INCLUDE__2023_10_18__PROXY_SERVER_HPP


#include "proxy/proxy_session.hpp"

#include <atomic>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>

#include <boost/json.hpp>


// jsonrpc 会话模板前置声明（launcher 控制通道的完整实现在 proxy_server.cpp）.
namespace jsonrpc {
	template <class StreamType> class jsonrpc_session;
}

namespace proxy {

	// launcher 控制通道状态结构体（完整定义在 proxy_server.cpp 中，避免在
	// 头文件中暴露其内部声明，相关成员函数均实现在 proxy_server.cpp）.
	struct launcher_state;

	//////////////////////////////////////////////////////////////////////////

	enum class pem_type
	{
		none,		// none.
		cert,		// certificate file.
		key,  		// certificate key file.
		pwd,		// certificate password file.
		dhparam		// dh param file.
	};

	struct pem_file
	{
		fs::path filepath_;
		pem_type type_ { pem_type::none };
		int chains_{ 0 };
	};

	struct certificate_file
	{
		pem_file cert_;
		pem_file key_;
		pem_file pwd_;
		pem_file dhparam_;

		std::string domain_;
		std::vector<std::string> subject_alt_name_;
		boost::posix_time::ptime expire_date_;

		std::optional<net::ssl::context> ssl_context_;
	};

	//////////////////////////////////////////////////////////////////////////

#ifdef __linux__
	// udp_tproxy_flow 结构体保存每个 UDP TPROXY flow 的状态信息, 包括客户端地址、原始目标地址和 relay socket 等等.
	struct udp_tproxy_flow
	{
		udp_tproxy_flow(const udp::endpoint& client_endp,
			const udp::endpoint& original_endp, size_t flow_key)
			: client_endp_(client_endp)
			, original_endp_(original_endp)
			, flow_key_(flow_key)
		{}

		// client_endp_ 保存客户端的地址.
		udp::endpoint client_endp_;

		// original_endp_ 保存客户端请求的原始目标地址.
		udp::endpoint original_endp_;

		// 每个 flow 创建一个用于转发数据包的 backend socket.
		// backend_sock_ 与上游代理服务器收发数据.
		std::optional<udp::socket> backend_sock_;

		// 每个 flow 创建一个用于转发数据包的 relay socket, relay_sock_ 用于伪装成目标服务器
		// 与客户端通信, 从而让客户端认为自己直接与目标服务器通信一样.
		std::optional<udp::socket> relay_sock_;

		// flow_key_ 是 flow 的唯一标识, 可以根据客户端地址和原始目标地址计算得到, 用于在 flow
		// 管理容器中快速查找对应的 flow.
		size_t flow_key_{ 0 };

		// expire 用于检查 flow 是否已过期.
		int expire_{ 0 };

		// 当使用 HTTP proxy_pass (RFC 9298 connect-udp) 时, 保存与上游的 TCP 连接.
		std::optional<variant_stream_type> udp_http_sock_;
		bool using_connect_udp_{ false };

		// 发送队列, 用于序列化 connect-udp 数据包的 TCP 发送, 避免多个并发协程同时写入
		// udp_http_sock_ 导致 capsule 数据在 TCP 流上交错损坏.
		std::deque<std::vector<char>> send_queue_;

		// 用于通知发送协程有新数据到达的定时器.
		// 当有新的 UDP 数据包推入 send_queue_ 时, 取消此定时器以唤醒发送协程.
		std::optional<net::steady_timer> notify_timer_;
	};
	using udp_tproxy_flow_ptr = std::shared_ptr<udp_tproxy_flow>;
#endif

	class proxy_server
		: public proxy_server_base
		, public std::enable_shared_from_this<proxy_server>
	{
		proxy_server(const proxy_server&) = delete;
		proxy_server& operator=(const proxy_server&) = delete;

		proxy_server(net::any_io_executor executor, proxy_server_option opt);

	public:
		// 创建 proxy_server 实例的方法.
		static std::shared_ptr<proxy_server>
		make(net::any_io_executor executor, proxy_server_option opt);

		// 析构函数（定义于 proxy_server.cpp, launcher_state 为不完整类型）.
		virtual ~proxy_server();

		// 验证 SSL 证书是否匹配 RFC 2818 的主机名规则.
		bool rfc2818_verification_match_pattern(
			const char* pattern, std::size_t pattern_length, const char* host);

		// 根据文件内容判断 PEM 文件类型 (cert/key/pwd/dhparam).
		pem_file determine_pem_type(const fs::path& filepath) noexcept;

		// 遍历证书目录, 收集所有证书文件信息.
		void walk_certificate(
			const fs::path& directory, std::vector<certificate_file>& certificates) noexcept;

		// 初始化 acceptor 并开始监听客户端连接.
		void init_acceptor() noexcept;

		// 更新证书列表 (重新加载证书文件).
		void update_certificate(
			const fs::path& directory, std::vector<certificate_file>& certificates) noexcept;

		// 初始化 SSL 上下文, 设置证书和回调.
		void init_ssl_context() noexcept;

		// ALPN 协议选择回调 (静态, 供 OpenSSL 调用).
		static int alpn_select_proto_cb(SSL *ssl, const unsigned char **out,
								unsigned char *outlen, const unsigned char *in,
								unsigned int inlen, void *arg);

		// ALPN 协议选择处理 (选择 http/1.1 协议).
		int alpn_select_proto(SSL *ssl, const unsigned char **out,
			unsigned char *outlen, const unsigned char *in,
			unsigned int inlen) noexcept;

		// SNI 回调 (静态, 供 OpenSSL 调用).
		static int ssl_sni_callback(SSL *ssl, int *ad, void *arg);

		// SNI 回调处理, 根据客户端 SNI 选择对应证书.
		int sni_callback(SSL *ssl, [[maybe_unused]] int *ad) noexcept;

		// 定时检查并更新过期证书.
		net::awaitable<std::chrono::seconds> certificate_check();

		// 定时器协程.
		net::awaitable<void> tick();

	public:
		// 启动代理服务, 开始监听客户端连接.
		void start() noexcept;

		// 关闭代理服务, 停止所有监听和会话.
		void close() noexcept;

		//////////////////////////////////////////////////////////////////////////
		// launcher 控制通道支持.

		// 实时状态快照（launcher status 上报）。
		boost::json::object snapshot_report();

		// 运行期应用配置（launcher set_config），返回 applied/needs_restart/errors。
		boost::json::object apply_options(const boost::json::object& options);

		// 添加/替换认证用户（user:password[:addr[:proxy_url]]）。
		bool add_auth_user(const std::string& user, const std::string& password,
			const std::string& addr, const std::string& proxy_url, std::string& err);

		// 删除认证用户，返回是否找到并删除。
		bool del_auth_user(const std::string& user);

		// 修改用户密码，返回是否找到用户。
		bool set_auth_user_password(const std::string& user, const std::string& password);

		// 设置单个用户的独立限速（rate<=0 取消限速）。
		bool set_auth_user_rate_limit(const std::string& user, int rate);

		// 设置单个用户的下载流量配额（quota<=0 取消配额）。
		bool set_auth_user_quota(const std::string& user, std::int64_t quota);

		// 续接 launcher 持久化的用户已用量（配额续接）。
		void set_user_usage(const boost::json::object& usage);

		// 当前用户状态（auth_users / users_rate_limit / users_quota）。
		boost::json::object users_state() const;

		// 服务启动时间（Unix 秒）。
		uint64_t started_at() const;

		// 服务版本标识。
		const std::string& server_version() const;

	private:
		// launcher 控制通道内部实现（全部协程, 运行于 m_executor）.

		// 启动 launcher 控制通道（由 start() 调用, URL 来自
		// m_option.launcher_url_, 为空则不启动）.
		void launcher_start() noexcept;

		// 停止 launcher 控制通道（由 close() 调用, 设置停止标志使协程退出）.
		void launcher_stop() noexcept;

		// 连接循环: 连接失败/断开后退避重连（全部协程, 不创建线程）.
		net::awaitable<void> launcher_worker();

		// 单次连接流程. 返回 true 表示成功建立了连接（尽管之后断开）.
		net::awaitable<bool> launcher_run_once();

		// 建立 ws/wss 连接并返回 JSON-RPC 会话；失败返回 nullopt.
		// 连接/握手全程受超时保护（超时后关闭 socket 使异步操作失败）.
		// 会话对象由调用方（launcher_run_once）持有, 后续均通过引用访问.
		template <typename WsStream>
		net::awaitable<std::optional<jsonrpc::jsonrpc_session<WsStream>>>
		launcher_connect(const std::string& host, const std::string& port,
			const std::string& target);

		// 一次连接的服务流程: 注册实例信息、启动读循环、状态上报循环,
		// 直到连接断开或 stop. 会话对象由调用方持有, 此处通过引用访问.
		template <typename WsStream>
		net::awaitable<void> launcher_serve(jsonrpc::jsonrpc_session<WsStream>& sess);

		// 注册 launcher → proxy_server 的请求/通知处理器.
		template <typename WsStream>
		void launcher_register_handlers(jsonrpc::jsonrpc_session<WsStream>& sess);

		// 协程方式处理一个请求, 分发到对应方法并回复（支持错误响应）.
		template <typename WsStream>
		net::awaitable<void> launcher_handle_request(jsonrpc::jsonrpc_session<WsStream>& sess, boost::json::object req);

		// 处理 launcher 下发的通知（无 id 消息, 如 set_user_usage）.
		net::awaitable<void> launcher_handle_notify(boost::json::object req);

		// 方法分发. 返回结果 json::value；失败抛出 launcher_error（定义于 .cpp）.
		boost::json::value launcher_dispatch(const std::string& method, const boost::json::value& params);

		// 采集快照、计算速率并上报.
		template <typename WsStream>
		void launcher_update_report(jsonrpc::jsonrpc_session<WsStream>& sess);

		// 从 --launcher URL 解析 instance ID.
		static std::string launcher_parse_instance_id(const std::string& url);

	private:
		// 移除指定 ID 的 session.
		void remove_session(size_t id) override;

		// 返回当前 session 数量.
		size_t num_session() override;

		// 返回当前服务器配置选项.
		const proxy_server_option& option() override;

		// 返回 SSL 上下文引用.
		net::ssl::context& ssl_context() override;

		// 会话结束时聚合累计流量（覆盖 proxy_server_base）。
		void session_closed(size_t id, uint64_t rx, uint64_t tx,
			const std::string& user) override;

	private:
		template <typename T, typename S>
		net::awaitable<void> start_accept(T& acceptor, S& socket);

		// start_proxy_listen 启动一个协程, 用于监听 proxy client 的连接.
		// 当有新的连接到来时, 会创建一个 proxy_session 对象, 并启动
		// proxy_session 对象.
		template <typename T>
		net::awaitable<void> start_proxy_listen(T& acceptor) noexcept;

		// 为指定 TCP acceptor 启动 32 个监听协程（非模板, 供 apply_options
		// 运行时 server_listen 热配置使用, 避免在模板定义前实例化模板）.
		void start_tcp_listen(tcp_acceptor& acceptor);

		// 设置透明代理, 获取客户端原始目标地址.
		net::awaitable<std::optional<net::ip::tcp::endpoint>>
		setup_tproxy(proxy_tcp_socket& socket, size_t connection_id) noexcept;

		// 获取当前机器所有本地 IP 地址.
		net::awaitable<void> get_local_address() noexcept;

		// 判断 IP 地址是否在指定的 CIDR 范围.
		bool ip_filter(const std::string& ip_cidr, const std::string& ip) const noexcept;

		// 根据地区信息过滤客户端连接 (白/黑名单).
		bool region_filter(const std::vector<std::string>& local_info) const noexcept;

		// 后端线程入口, 用于处理同步转异步操作.
		void backend_thread_run() noexcept;

		// 切换到后端执行上下文（非锁定调度时）.
		net::awaitable<net::any_io_executor> switch_to_backend_executor();

		// 从后端执行上下文切换回主执行上下文.
		net::awaitable<void> switch_from_backend_executor();

#if defined(__linux__)
		// 从 msg 中提取原客户端和原目标地址.
		static bool parse_udp_tproxy_packet(struct msghdr& msg,
			udp::endpoint& client_ep, udp::endpoint& original_dest);

		// 用 (client_ep, original_dest) 计算查找 flow 的 key.
		static size_t make_udp_flow_key(const udp::endpoint& client, const udp::endpoint& dest);

		// 清理过期的 UDP TPROXY flow.
		net::awaitable<void> udp_tproxy_check() noexcept;

		// 启动 UDP 透明代理监听.
		net::awaitable<void> start_udp_tproxy() noexcept;

		// 发起 UDP 向上游 socks5 代理服务器的连接.
		net::awaitable<void> udp_tproxy_socks5_connect() noexcept;

		// 解析 proxy_pass 地址并返回 endpoints.
		net::awaitable<std::optional<tcp::resolver::results_type>>
		resolve_proxy_pass(const boost::urls::url& proxy_pass);

		// 如果配置了 SO_MARK, 则对指定 socket 应用标记.
		boost::system::result<void>
		apply_so_mark(int fd) noexcept;

		// 连接到上游代理服务器.
		net::awaitable<boost::system::error_code>
		connect_to_proxy(tcp::socket& remote_socket, const tcp::resolver::results_type& targets);

		// 创建 ssl socket.
		net::awaitable<boost::system::result<bool>>
		make_ssl_socket(tcp::socket& remote_socket,
			std::string_view sni, std::optional<variant_stream_type>& ssl_sock);

		// 执行 SOCKS5 UDP ASSOCIATE 握手.
		net::awaitable<bool> do_socks5_associate();

		// UDP TPROXY 响应循环, 从 upstream 接收数据并转发回客户端.
		net::awaitable<void> udp_tproxy_response_loop(udp_tproxy_flow_ptr flow);

		// UDP TPROXY 使用 RFC 9298 connect-udp 转发循环.
		net::awaitable<void> udp_tproxy_http_udp_loop(udp_tproxy_flow_ptr flow);

		// 初始化 relay socket.
		bool init_relay_socket(udp_tproxy_flow_ptr flow);

		// 去掉 SOCKS5 UDP 头, 然后使用 sendmsg + IP_PKTINFO 将原始数据送回客户端.
		void send_response_to_client(udp_tproxy_flow_ptr flow, const char* data, std::size_t len);

		// 将客户端数据通过 relay socket 转发到上游代理.
		void udp_tproxy_forward_packet(
			udp_tproxy_flow_ptr flow, const char* data, std::size_t len);

		// UDP TPROXY 使用 connect-udp 转发数据包 (RFC 9298 capsule).
		void udp_tproxy_forward_packet_http(
			udp_tproxy_flow_ptr flow, const char* data, std::size_t len);

		// 启动 UDP TPROXY 监听协程.
		net::awaitable<void> start_udp_tproxy_listen(udp::socket& udp_sock) noexcept;

#endif // defined(__linux__)

	private:
		// m_executor 保存当前 io_context 的 executor.
		net::any_io_executor m_executor;

		// 用于处理一些同步转异步操作的 io_context.
		// 如执行 dns 解析时, 实际上并不是异步的, 需要在线程池中执行同步操作,
		// 然后切换回当前 io_context 继续执行异步操作.
		net::io_context m_backend_context{ 1 };

		// 用于运行 m_backend_context 的线程.
		std::unique_ptr<std::thread> m_backend_thread;

		// 作为中继桥接的时候, 下游代理服务器解析的地址缓存.
		dns_cache m_dns_cache{ 128 };

		// 记录 asio 调度器是否启用锁标识.
		bool m_scheduler_locking;

		// m_tcp_acceptors 用于侦听客户端 tcp 连接请求.
		// 使用 deque<unique_ptr> 持有: 元素在堆上分配, 监听协程通过引用绑定
		// 堆对象, 增删元素时引用地址保持稳定, 避免 vector 重分配导致悬空引用.
		std::deque<std::unique_ptr<tcp_acceptor>> m_tcp_acceptors;
		// 已关闭并从 m_tcp_acceptors 移除的 acceptor, 暂存于此保持对象存活,
		// 直到其监听协程退出后自然消亡, 避免 use-after-free.
		std::deque<std::unique_ptr<tcp_acceptor>> m_closed_tcp_acceptors;
		// m_unix_acceptors 用于侦听客户端 UDS 连接请求.
		std::vector<unix_acceptor> m_unix_acceptors;

		// m_option 保存当前服务器各选项配置.
		mutable std::mutex m_option_mutex;
		proxy_server_option m_option;

		// 当前机器的所有 ip 地址.
		std::set<net::ip::address> m_local_addrs;

		// ipip 用于获取 ip 地址的地理位置信息.
		std::unique_ptr<ip_database> m_ip_database;

		using proxy_session_weak_ptr =
			std::weak_ptr<proxy_session>;

		// 当前客户端连接列表.
		std::unordered_map<size_t, proxy_session_weak_ptr> m_sessions;

		// 保护 m_sessions（agent 状态上报线程与 io_context 线程并发访问）。
		std::mutex m_sessions_mutex;

		// 当前服务端作为 ssl 服务时的 ssl context.
		net::ssl::context m_ssl_srv_context{ net::ssl::context::tls_server };

		// m_certificates 保存当前服务端的证书信息.
		std::atomic<std::vector<certificate_file>*> m_certificates{ nullptr };
		std::vector<certificate_file> m_certificate_master;
		std::vector<certificate_file> m_certificate_slave;

		net::steady_timer m_timer;

#if defined(__linux__)
		// UDP TPROXY 透明代理相关成员.
		std::vector<udp::socket> m_udp_tproxy_listeners;

		// 存储每个 UDP TPROXY flow 的状态信息, 包括客户端地址、原始目标地址和 backend socket 等等.
		std::unordered_map<size_t, udp_tproxy_flow_ptr> m_udp_tproxy_flows;

		// proxy_pass 返回侦听的 UDP 端口地址信息, 所有 UDP TPROXY
		// flow 共享这个地址信息将数据转发到 proxy_pass.
		udp::endpoint m_backend_endp;

		// 重试 UDP TPROXY socks5 连接.
		std::atomic_bool m_retry_tproxy_socks5_connect = { false };
#endif // defined(__linux__)

		//////////////////////////////////////////////////////////////////////////
		// launcher 控制通道状态：启动时间/版本标识/全局与用户流量统计、控制通道
		// 地址、停止标志、在途请求计数、SSL 上下文与最近状态报告等.
		std::unique_ptr<launcher_state> m_launcher_state;

		// 当前服务是否中止标志.
		std::atomic_bool m_abort{ false };
	};

}

#endif // INCLUDE__2023_10_18__PROXY_SERVER_HPP
