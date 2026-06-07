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

#ifdef __linux__
	// udp_tproxy_flow 结构体保存每个 UDP TPROXY flow 的状态信息, 包括客户端地址、原始目标地址和 relay socket 等等.
	struct udp_tproxy_flow
	{
		udp_tproxy_flow(const udp::endpoint& client_endp,
			const udp::endpoint& original_endp, udp::socket& tproxy_sock)
			: client_endp_(client_endp)
			, original_endp_(original_endp)
			, tproxy_sock_(tproxy_sock)
		{}

		// client_endp_ 保存客户端的地址.
		udp::endpoint client_endp_;

		// original_endp_ 保存客户端请求的原始目标地址.
		udp::endpoint original_endp_;

		// 每个 flow 创建一个用于转发数据包的 relay socket.
		std::optional<udp::socket> relay_sock_;

		// tproxy_sock_ 保存 tproxy 模式下用于接收客户端数据包的 socket 的引用, 用于在 flow 中转发数据包时使用.
		udp::socket& tproxy_sock_;

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
		static std::shared_ptr<proxy_server>
		make(net::any_io_executor executor, proxy_server_option opt);

		virtual ~proxy_server() = default;

		bool rfc2818_verification_match_pattern(
			const char* pattern, std::size_t pattern_length, const char* host);

		pem_file determine_pem_type(const fs::path& filepath) noexcept;

		void walk_certificate(
			const fs::path& directory, std::vector<certificate_file>& certificates) noexcept;

#if defined(__linux__)
#  if !defined(IP_TRANSPARENT)
#    define IP_TRANSPARENT 19
#  endif
#  if !defined(IPV6_TRANSPARENT)
#    define IPV6_TRANSPARENT 75
#  endif
#  if !defined(IP_RECVORIGDSTADDR)
#    define IP_RECVORIGDSTADDR 20
#  endif
#  if !defined(IP_ORIGDSTADDR)
#    define IP_ORIGDSTADDR 20
#  endif
#  if !defined(IPV6_RECVORIGDSTADDR)
#    define IPV6_RECVORIGDSTADDR 74
#  endif
#  if !defined(IPV6_ORIGDSTADDR)
#    define IPV6_ORIGDSTADDR 74
#  endif
#endif // defined(__linux__)

		void init_acceptor() noexcept;

		void update_certificate(
			const fs::path& directory, std::vector<certificate_file>& certificates) noexcept;

		void init_ssl_context() noexcept;

		static int alpn_select_proto_cb(SSL *ssl, const unsigned char **out,
                                unsigned char *outlen, const unsigned char *in,
                                unsigned int inlen, void *arg);

		int alpn_select_proto(SSL *ssl, const unsigned char **out,
			unsigned char *outlen, const unsigned char *in,
			unsigned int inlen) noexcept;

		static int ssl_sni_callback(SSL *ssl, int *ad, void *arg);

		int sni_callback(SSL *ssl, [[maybe_unused]] int *ad) noexcept;

		net::awaitable<void> certificate_check_timer();

	public:
		void start() noexcept;

		void close() noexcept;

	private:
		void remove_session(size_t id) override;

		size_t num_session() override;

		const proxy_server_option& option() override;

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
		// 当有新的连接到来时, 会创建一个 proxy_session 对象, 并启动 proxy_session
		// 的对象.
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

		net::awaitable<std::optional<net::ip::tcp::endpoint>>
		setup_tproxy(proxy_tcp_socket& socket, size_t connection_id) noexcept;

		net::awaitable<void> get_local_address() noexcept;

		// 判断 IP 地址是否在指定的 CIDR 范围.
		bool ip_filter(const std::string& ip_cidr, const std::string& ip) const noexcept;

		bool region_filter(const std::vector<std::string>& local_info) const noexcept;

		void backend_thread_run() noexcept;

#if defined(__linux__)
		// msg 不能为 const，CMSG_NXTHDR 需要非 const msghdr*.
		static bool parse_udp_tproxy_packet(struct msghdr& msg, ssize_t ret,
			udp::endpoint& client_ep, udp::endpoint& original_dest);

		// 为 (client_ep, original_dest) 生成查找 key.
		static size_t make_udp_flow_key(const udp::endpoint& client, const udp::endpoint& dest);

		net::awaitable<void> udp_tproxy_check() noexcept;

		net::awaitable<void> start_udp_tproxy() noexcept;

		// 解析 proxy_pass 地址并返回 endpoints.
		net::awaitable<std::optional<tcp::resolver::results_type>>
		resolve_proxy_pass(const boost::urls::url& proxy_pass);

		net::awaitable<boost::system::error_code>
		connect_to_proxy(tcp::socket& remote_socket, const tcp::resolver::results_type& targets);

		net::awaitable<bool> do_sock5_associate();

		net::awaitable<void> udp_tproxy_response_loop(udp_tproxy_flow_ptr flow);

		// 去掉 SOCKS5 UDP 头, 然后使用 sendmsg + IP_PKTINFO 将原始数据送回客户端.
		void send_response_to_client(udp_tproxy_flow_ptr flow, const char* data, std::size_t len);

		void udp_tproxy_forward_packet(
			udp_tproxy_flow_ptr flow, const char* data, std::size_t len);

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

		// 存储每个 UDP TPROXY flow 的状态信息, 包括客户端地址、原始目标地址和 relay socket 等等.
		std::unordered_map<size_t, udp_tproxy_flow_ptr> m_udp_tproxy_flows;

		// proxy_pass 返回侦听的 UDP 端口地址信息, 所有 UDP TPROXY
		// flow 共享这个地址信息将数据转发到 proxy_pass.
		udp::endpoint m_relay_endp;
#endif // defined(__linux__)

		// 当前服务是否中止标志.
		bool m_abort{ false };
	};

}

#endif // INCLUDE__2023_10_18__PROXY_SERVER_HPP
