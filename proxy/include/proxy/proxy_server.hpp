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


namespace proxy {

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
			const udp::endpoint& original_endp, udp::socket& tproxy_sock, size_t flow_key)
			: client_endp_(client_endp)
			, original_endp_(original_endp)
			, tproxy_sock_(tproxy_sock)
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

		// tproxy_sock_ 保存 tproxy 模式下用于接收客户端数据包的 socket 的引用, 用于在 flow
		// 中转发数据包时使用.
		udp::socket& tproxy_sock_;

		// flow_key_ 是 flow 的唯一标识, 可以根据客户端地址和原始目标地址计算得到, 用于在 flow
		// 管理容器中快速查找对应的 flow.
		size_t flow_key_{ 0 };

		// expire 用于检查 flow 是否已过期.
		int expire_{ 0 };
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

		virtual ~proxy_server() = default;

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
		net::awaitable<void> certificate_check_timer();

	public:
		// 启动代理服务, 开始监听客户端连接.
		void start() noexcept;

		// 关闭代理服务, 停止所有监听和会话.
		void close() noexcept;

	private:
		// 移除指定 ID 的 session.
		void remove_session(size_t id) override;

		// 返回当前 session 数量.
		size_t num_session() override;

		// 返回当前服务器配置选项.
		const proxy_server_option& option() override;

		// 返回 SSL 上下文引用.
		net::ssl::context& ssl_context() override;

	private:
		template <typename T, typename S>
		net::awaitable<void> start_accept(T& acceptor, S& socket)
		{
			boost::system::error_code error;
			net::socket_base::keep_alive keep_alive_opt(true);
			net::ip::tcp::no_delay no_delay_opt(true);
			net::ip::tcp::no_delay delay_opt(false);

			auto self = shared_from_this();

			co_await acceptor.async_accept(socket.lowest_layer(), net_awaitable[error]);
			if (error)
			{
				if (error == net::error::operation_aborted || m_abort)
					co_return;

				XLOG_WARN << "start_proxy_listen, async_accept: " << error.message();
				co_return;
			}

			static std::atomic_size_t id{ 1 };
			size_t connection_id = id++;

			std::vector<std::string> local_info;
			std::string client;

			if constexpr (std::same_as<S, proxy_tcp_socket>)
			{
				auto endp = tcp_remote_endpoint(socket);
				client = endp.address().to_string();
				local_info.push_back(client);
				client += ":" + std::to_string(endp.port());

				if (m_ipdb)
				{
					try {
						auto [ret, isp] = m_ipdb->lookup(endp.address());
						if (!ret.empty())
						{
							for (auto& c : ret)
								client += " " + c;

							local_info.insert(local_info.end(), ret.begin(), ret.end());
						}
					}
					catch (const std::exception&)
					{}
				}
			}
			else if constexpr (std::same_as<S, proxy_uds_socket>)
			{
				auto endp = uds_remote_endpoint(socket);
				client = endp.path();
			}

			XLOG_DBG << "connection id: " << connection_id
				<< ", start client incoming: " << client;

			if (!region_filter(local_info))
			{
				XLOG_WARN << "connection id: " << connection_id
					<< ", region filter: " << client;
				co_return;
			}

			socket.set_option(keep_alive_opt, error);

#if defined (__linux__)
			std::optional<net::ip::tcp::endpoint> tproxy_endpoint;
			// 是否启用透明代理.
			if constexpr (std::same_as<S, proxy_tcp_socket>)
			{
				if (m_option.transparent_)
					tproxy_endpoint = co_await setup_tproxy(socket, connection_id);
			}
#endif

			// 在启用 scramble 时, 刻意开启 Nagle's algorithm 以尽量保证数据包
			// 被重组, 尽最大可能避免观察者通过观察 ip 数据包大小的规律来分析 tcp
			// 数据发送调用, 从而增加噪声加扰的强度.
			if (m_option.scramble_)
				socket.set_option(delay_opt, error);
			else
				socket.set_option(no_delay_opt, error);

			// 创建 proxy_session 对象.
			auto new_session =
				std::make_shared<proxy_session>(
					m_executor,
					m_backend_context,
					m_scheduler_locking,
					m_dns_cache,
					init_proxy_stream(std::move(socket)),
					connection_id,
					self);

			// 保存 proxy_session 对象到 m_clients 中.
			m_clients[connection_id] = new_session;

#if defined (__linux__)
			if constexpr (std::same_as<S, proxy_tcp_socket>)
			{
				if (tproxy_endpoint)
					new_session->setup_tproxy(*tproxy_endpoint);
			}
#endif

			// 启动 proxy_session 对象.
			new_session->start();

			co_return;
		}

		// start_proxy_listen 启动一个协程, 用于监听 proxy client 的连接.
		// 当有新的连接到来时, 会创建一个 proxy_session 对象, 并启动
		// proxy_session 对象.
		template <typename T>
		net::awaitable<void> start_proxy_listen(T& acceptor) noexcept
		{
			auto self = shared_from_this();

			while (!m_abort)
			{
				if constexpr (std::same_as<std::decay_t<T>, tcp_acceptor>)
				{
					proxy_tcp_socket socket(m_executor);
					co_await start_accept(acceptor, socket);
				}
				else if constexpr (std::same_as<std::decay_t<T>, unix_acceptor>)
				{
					proxy_uds_socket socket(m_executor);
					co_await start_accept(acceptor, socket);
				}
			}

			XLOG_WARN << "start_proxy_listen exit ...";
			co_return;
		}

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

#if defined(__linux__)
		// msg 不能为 const，CMSG_NXTHDR 需要非 const msghdr*.
		static bool parse_udp_tproxy_packet(struct msghdr& msg, ssize_t ret,
			udp::endpoint& client_ep, udp::endpoint& original_dest);

		// 为 (client_ep, original_dest) 生成查找 key.
		static size_t make_udp_flow_key(const udp::endpoint& client, const udp::endpoint& dest);

		// 清理过期的 UDP TPROXY flow.
		net::awaitable<void> udp_tproxy_check() noexcept;

		// 启动 UDP 透明代理监听.
		net::awaitable<void> start_udp_tproxy() noexcept;

		// 解析 proxy_pass 地址并返回 endpoints.
		net::awaitable<std::optional<tcp::resolver::results_type>>
		resolve_proxy_pass(const boost::urls::url& proxy_pass);

		// 连接到上游代理服务器.
		net::awaitable<boost::system::error_code>
		connect_to_proxy(tcp::socket& remote_socket, const tcp::resolver::results_type& targets);

		// 执行 SOCKS5 UDP ASSOCIATE 握手, 获取 relay endpoint.
		net::awaitable<bool> do_sock5_associate();

		// UDP TPROXY 响应循环, 从 upstream 接收数据并转发回客户端.
		net::awaitable<void> udp_tproxy_response_loop(udp_tproxy_flow_ptr flow);

		// 初始化 relay socket.
		bool init_relay_socket(udp_tproxy_flow_ptr flow);

		// 去掉 SOCKS5 UDP 头, 然后使用 sendmsg + IP_PKTINFO 将原始数据送回客户端.
		void send_response_to_client(udp_tproxy_flow_ptr flow, const char* data, std::size_t len);

		// 将客户端数据通过 relay socket 转发到上游代理.
		void udp_tproxy_forward_packet(
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
		std::vector<tcp_acceptor> m_tcp_acceptors;
		// m_unix_acceptors 用于侦听客户端 UDS 连接请求.
		std::vector<unix_acceptor> m_unix_acceptors;

		// m_option 保存当前服务器各选项配置.
		proxy_server_option m_option;

		// 当前机器的所有 ip 地址.
		std::set<net::ip::address> m_local_addrs;

		// ipip 用于获取 ip 地址的地理位置信息.
		std::unique_ptr<ip_database> m_ipdb;

		using proxy_session_weak_ptr =
			std::weak_ptr<proxy_session>;

		// 当前客户端连接列表.
		std::unordered_map<size_t, proxy_session_weak_ptr> m_clients;

		// 当前服务端作为 ssl 服务时的 ssl context.
		net::ssl::context m_ssl_srv_context{ net::ssl::context::tls_server };

		// m_certificates 保存当前服务端的证书信息.
		std::vector<certificate_file>* m_certificates{ nullptr };
		std::vector<certificate_file> m_certificate_master;
		std::vector<certificate_file> m_certificate_slave;

		net::steady_timer m_timer;

#if defined(__linux__)
		// UDP TPROXY 透明代理相关成员.
		std::vector<udp::socket> m_udp_tproxy_listeners;

		std::mutex m_udp_flows_mutex;

		// 存储每个 UDP TPROXY flow 的状态信息, 包括客户端地址、原始目标地址和 backend socket 等等.
		std::unordered_map<size_t, udp_tproxy_flow_ptr> m_udp_tproxy_flows;

		// proxy_pass 返回侦听的 UDP 端口地址信息, 所有 UDP TPROXY
		// flow 共享这个地址信息将数据转发到 proxy_pass.
		udp::endpoint m_backend_endp;
#endif // defined(__linux__)

		// 当前服务是否中止标志.
		bool m_abort{ false };
	};

}

#endif // INCLUDE__2023_10_18__PROXY_SERVER_HPP
