//
// proxy_session_connect.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/proxy_session.hpp"
#include "proxy/http_error_body.hpp"
#include "proxy/proxy_websocket_client.hpp"

namespace proxy {

	//////////////////////////////////////////////////////////////////////////
	// 连接相关

	net::awaitable<boost::system::error_code>
	proxy_session::connect_proxy_pass(tcp::socket& remote_socket, tcp::resolver::results_type targets) noexcept
	{
		auto proxy_hostname = m_proxy_pass->encoded_origin();

		log_conn_debug()
			<< ", starting connection to next proxy: "
			<< proxy_hostname;

		auto ec = co_await async_connect_targets(remote_socket, targets);
		if (ec)
		{
			log_conn_warning()
				<< ", connect to next proxy: "
				<< proxy_hostname
				<< ", error: "
				<< ec.message();

			co_return ec;
		}

		log_conn_debug()
			<< ", connect to next proxy: '"
			<< proxy_hostname
			<< "' successfully";

		co_return ec;
	}

	net::awaitable<boost::system::error_code>
	proxy_session::proxy_pass_handshake(tcp::socket& remote_socket,
		const std::string& target_host, uint16_t target_port,
		const urls::url& proxy_url, int command)
	{
		auto proxy_hostname = proxy_url.encoded_origin();

		// 如果启用了 noise, 则在向上游代理服务器发起 tcp 连接成功后, 发送 noise
		// 数据以及接收 noise 数据进行握手.
		if (m_option.scramble_)
		{
			if (!co_await noise_handshake(remote_socket, m_client_rx_key, m_client_tx_key))
				co_return make_error_code(boost::system::errc::io_error);

			log_conn_debug()
				<< ", next proxy: " << proxy_hostname
				<< ", with upstream noise completed";
		}

		auto proxy_pass_use_ssl = m_option.proxy_pass_use_ssl_;
		auto scheme = boost::to_lower_copy(std::string(proxy_url.scheme()));

		// 判断是否使用 ssl 加密与下一级代理通信.
		// 这里仅当 scheme 为 socks协议后辍是 s 时启用 ssl 加密.
		// 其它 scheme 仍按 scheme 含义中是否包括 ssl 加密来处理.
		if (proxy_scheme_use_ssl(scheme))
			proxy_pass_use_ssl = true;

		// 初始化 m_remote_socket 为 variant_stream_type 类型, 若是 ssl/tls 加
		// 密, 则初始化为 ssl_tcp_stream.
		m_remote_socket = std::move(
			co_await instantiate_proxy_pass(std::move(remote_socket), proxy_pass_use_ssl));

		log_conn_debug()
			<< ", next proxy: "
			<< proxy_hostname
			<< ", handshake with "
			<< std::string(scheme)
			<< ", target: "
			<< target_host
			<< ":"
			<< target_port;

		boost::system::error_code ec;

		// 根据 scheme 向 proxy_pass 进行代理握手相关实现.
		if (scheme.starts_with("socks"))
		{
			socks_client_option opt;

			opt.target_host = target_host;
			opt.target_port = target_port;
			opt.proxy_hostname = is_hostname(target_host);
			opt.command = command;
			opt.username = std::string(proxy_url.user());
			opt.password = std::string(proxy_url.password());

			if (scheme.starts_with("socks4a"))
				opt.version = socks4a_version;
			else if (scheme.starts_with("socks4"))
				opt.version = socks4_version;

			auto endpoint = co_await async_socks_handshake(
				m_remote_socket,
				opt,
				net_awaitable[ec]);
			if (endpoint)
			{
				m_remote_udp_endpoint = *endpoint;
				if (m_remote_udp_endpoint.address() == net::ip::make_address_v4("0.0.0.0") ||
					m_remote_udp_endpoint.address() == net::ip::make_address_v6("::0"))
				{
					m_remote_udp_endpoint.address(tcp_remote_endpoint(m_remote_socket).address());
				}
			}
		}
		else if (scheme.starts_with("http"))
		{
			http_proxy_client_option opt;

			opt.target_host = target_host;
			opt.target_port = target_port;
			opt.username = std::string(proxy_url.user());
			opt.password = std::string(proxy_url.password());

			if (!m_option.scramble_ && (m_option.noise_length_ >= 16 && m_option.noise_length_ <= 64 * 1024))
			{
				// 如果配置了 noise_length_ 则启用 http 代理握手中的 padding, 以迷惑扫描者无法通过
				// tcp 数据发送长度特征来识别出是 http 代理握手数据.
				std::mt19937 gen(global_random_device()());
				std::uniform_int_distribution<int64_t> dis(16, m_option.noise_length_);

				auto padding = generate_noise(global_random_device(), static_cast<uint16_t>(dis(gen)));
				auto len = boost::beast::detail::base64::encoded_size(padding.size());
				std::string padding_value(len, 0);
				len = beast::detail::base64::encode(padding_value.data(), padding.data(), padding.size());
				padding_value.resize(len);

				opt.extra_headers.insert("Proxy-Padding", padding_value);
			}

			if (command == SOCKS5_CMD_UDP)
			{
				ec = net::error::operation_not_supported;
			}
			else
			{
				co_await async_http_proxy_handshake(
					m_remote_socket,
					opt,
					net_awaitable[ec],
					&m_remote_buffer);
			}
		}
		else if (scheme == "ws" || scheme == "wss")
		{
			if (command == SOCKS5_CMD_UDP)
			{
				ec = net::error::operation_not_supported;
			}
			else
			{
				auto options = make_websocket_handshake_options(
					proxy_url, target_host, target_port);
				ec = co_await async_websocket_proxy_handshake(
					m_remote_socket, std::move(options));
			}
		}

		if (ec)
		{
			log_conn_warning()
				<< ", next proxy: "
				<< proxy_hostname
				<< ", handshake error: "
				<< ec.message();

			co_return ec;
		}

		co_return ec;
	}

	net::awaitable<boost::system::error_code> proxy_session::start_connect_host(
		std::string target_host,
		uint16_t target_port,
		bool resolved,
		int command) noexcept
	{
		// 记录本次会话的代理目标（供 launcher 连接列表上报）.
		set_target(target_host + ":" + std::to_string(target_port));

		tcp::socket& remote_socket = net_tcp_socket(m_remote_socket);
		tcp::resolver::results_type targets;
		boost::system::error_code ec;

		if (m_proxy_pass)
		{
			// 查询网关代理中继服务器域名信息.
			targets = co_await resolve_proxy_pass_targets();

			// 发起连接到网关代理中继服务器.
			ec = co_await connect_proxy_pass(remote_socket, targets);
			if (!ec)
			{
				// 与网关代理中继服务器握手.
				ec = co_await proxy_pass_handshake(remote_socket,
					target_host, target_port,
					*m_proxy_pass, command);
			}

			co_return ec;
		}
		else
		{
			switch (static_cast<int>(resolved))
			{
			case 1: // true
			{
				tcp::endpoint dst_endpoint;

				auto addr = net::ip::make_address(target_host, ec);
				if (!ec)
				{
					dst_endpoint.address(addr);
					dst_endpoint.port(target_port);

					targets = net::ip::basic_resolver_results<tcp>::create(dst_endpoint, "", "");
					break;
				}
			}
			[[fallthrough]];
			case 0:  // false
			default:
				targets = co_await resolve_targets(target_host, target_port);
				break;
			}
		}

		// 直接连接到目标机器.
		ec = co_await async_connect_targets(remote_socket, targets);
		if (ec)
			co_return ec;

		// 重新初始化 m_remote_socket 为 variant_stream_type 类型.
		m_remote_socket = init_proxy_stream(std::move(remote_socket));

		co_return ec;
	}

	//////////////////////////////////////////////////////////////////////////
	// DNS 解析与缓存

	tcp::resolver::results_type
	proxy_session::get_resolver_from_cache(const std::string& hostname, uint16_t port) noexcept
	{
		auto dns = m_dns_cache.get(hostname);
		if (dns)
		{
			[[maybe_unused]] auto& [result, tp] = *dns;

			std::vector<tcp::endpoint> new_endpoints;
			new_endpoints.reserve(result.size());

			std::transform(result.begin(), result.end(),
				std::back_inserter(new_endpoints),
				[port](const tcp::endpoint& ep)
				{
					return tcp::endpoint(ep.address(), port);
				});

			return tcp::resolver::results_type::create(
				new_endpoints.begin(), new_endpoints.end(),
				hostname, std::to_string(port));
		}

		return {};
	}

	net::awaitable<net::any_io_executor> proxy_session::switch_to_backend_executor()
	{
		co_return co_await backend_switch_to(
			m_scheduler_locking, m_backend_context, m_executor);
	}

	net::awaitable<void> proxy_session::switch_from_backend_executor()
	{
		co_await backend_switch_from(m_scheduler_locking, m_executor);
	}

	net::awaitable<tcp::resolver::results_type>
	proxy_session::resolve_targets(const std::string& hostname, uint16_t port_number) noexcept
	{
		net::ip::basic_resolver_results<tcp> targets;

		targets = get_resolver_from_cache(hostname, port_number);
		if (!targets.empty())
			co_return targets;

		boost::system::error_code ec;

		auto executor = co_await switch_to_backend_executor();

		tcp::resolver resolver{ executor };

		targets = co_await resolver.async_resolve(
			hostname,
			std::to_string(port_number),
			net_awaitable[ec]);

		co_await switch_from_backend_executor();

		if (ec)
		{
			log_conn_warning()
				<< ", resolve: "
				<< hostname
				<< ", error: "
				<< ec.message();

			co_return targets;
		}

		// 更新 dns 缓存.
		dns_result result = std::make_tuple(targets, std::chrono::steady_clock::now());
		m_dns_cache.put(hostname, std::move(result));

		co_return targets;
	}

	net::awaitable<tcp::resolver::results_type>
	proxy_session::resolve_proxy_pass_targets() noexcept
	{
		tcp::resolver::results_type targets;

		if (!m_proxy_pass)
			co_return targets;

		auto proxy_host = std::string(m_proxy_pass->host_address());
		uint16_t proxy_port_number = 0;

		if (m_proxy_pass->port_number() == 0)
			proxy_port_number = proxy_pass_default_port(*m_proxy_pass);
		else
			proxy_port_number = m_proxy_pass->port_number();

		// 如果是 IP 地址, 则直接构造而不需要 DNS 查询.
		if (!is_hostname(proxy_host))
		{
			net::ip::tcp::endpoint endp(
				net::ip::make_address(proxy_host), proxy_port_number);

			targets = tcp::resolver::results_type::create(
				endp, proxy_host, m_proxy_pass->scheme());
		}
		else
		{
			// 进行 DNS 查询.
			targets = co_await resolve_targets(proxy_host, proxy_port_number);
		}

		co_return targets;
	}

	//////////////////////////////////////////////////////////////////////////

	net::awaitable<boost::system::error_code>
	proxy_session::async_connect_targets(tcp::socket& socket, tcp::resolver::results_type& targets) noexcept
	{
		boost::system::error_code ec;

		if (targets.empty())
		{
			ec = net::error::host_not_found;
			co_return ec;
		}

		if (m_option.happyeyeballs_)
		{
			co_await asio_util::async_connect(
				socket,
				targets,
				[this](const auto&, auto& stream, auto& endp)
				{
					boost::system::error_code ec;

					if (m_bind_interface)
					{
						tcp::endpoint bind_endpoint(*m_bind_interface, 0);

						stream.open(bind_endpoint.protocol(), ec);
						if (ec)
							return false;

						stream.bind(bind_endpoint, ec);
						if (ec)
							return false;
					}
					else
					{
						stream.open(endp.endpoint().protocol(), ec);
						if (ec)
							return false;
					}

					// 路由决策发生在 connect 时，须在连接前设置 SO_MARK.
					if (m_option.so_mark_)
					{
						auto ret = apply_so_mark(stream.native_handle(), m_option.so_mark_);
						if (ret.has_error())
						{
							log_conn_warning()
								<< ", set socket mark error: "
								<< ret.error().message();
						}
					}

					return true;
				},
				net_awaitable[ec]);

			{
				auto ret = set_tcp_keepalive(socket.native_handle());
				if (ret.has_error())
				{
					log_conn_warning()
						<< ", tcp keepalive error: "
						<< ret.error().message();
				}
			}

			// 禁用 Nagle 算法, 使小数据包(如 HTTP 头、SOCKS 握手)立即发送.
			{
				auto ret = set_tcp_nodelay(socket.native_handle(), true);
				if (ret.has_error())
				{
					log_conn_warning()
						<< ", tcp nodelay error: "
						<< ret.error().message();
				}
			}

#if defined(__linux__)
			// 启用 TCP_QUICKACK, 减少 ACK 延迟, 提升交互式代理场景的响应速率.
			{
				auto ret = set_tcp_quickack(socket.native_handle(), true);
				if (ret.has_error())
				{
					log_conn_warning()
						<< ", tcp quickack error: "
						<< ret.error().message();
				}
			}
#endif

			// 增大 TCP 缓冲区以提升吞吐量 (256KB 接收, 256KB 发送).
			{
				auto ret = set_tcp_buffer_sizes(
					socket.native_handle(), 256 * 1024, 256 * 1024);
				if (ret.has_error())
				{
					log_conn_warning()
						<< ", tcp buffer sizes error: "
						<< ret.error().message();
				}
			}

			co_return ec;
		}

		for (const auto& endpoint : targets)
		{
			ec = boost::asio::error::host_not_found;

			if (m_option.connect_v4_only_)
			{
				if (endpoint.endpoint().address().is_v6())
				{
					XLOG_DBG << "connect_v4_only: skip IPv6 endpoint "
						<< endpoint.endpoint();
					continue;
				}
			}
			else if (m_option.connect_v6_only_)
			{
				if (endpoint.endpoint().address().is_v4())
				{
					XLOG_DBG << "connect_v6_only: skip IPv4 endpoint "
						<< endpoint.endpoint();
					continue;
				}
			}

			boost::system::error_code ignore_ec;
			socket.close(ignore_ec);

			if (m_bind_interface)
			{
				tcp::endpoint bind_endpoint(
					*m_bind_interface,
					0);

				socket.open(
					bind_endpoint.protocol(),
					ec);
				if (ec)
					break;

				socket.bind(
					bind_endpoint,
					ec);
				if (ec)
					break;
			}
			else
			{
				socket.open(endpoint.endpoint().protocol(), ec);
				if (ec)
					break;
			}

			// 路由决策发生在 connect 时，须在连接前设置 SO_MARK.
			if (m_option.so_mark_)
			{
				auto ret = apply_so_mark(socket.native_handle(), m_option.so_mark_);
				if (ret.has_error())
				{
					log_conn_warning()
						<< ", set socket mark error: "
						<< ret.error().message();
				}
			}

			co_await socket.async_connect(
				endpoint,
				net_awaitable[ec]);
			if (!ec)
			{
				{
					auto ret = set_tcp_keepalive(socket.native_handle());
					if (ret.has_error())
					{
						log_conn_warning()
							<< ", tcp keepalive error: "
							<< ret.error().message();
					}
				}

				// 禁用 Nagle 算法, 使小数据包立即发送.
				{
					auto ret = set_tcp_nodelay(socket.native_handle(), true);
					if (ret.has_error())
					{
						log_conn_warning()
							<< ", tcp nodelay error: "
							<< ret.error().message();
					}
				}

#if defined(__linux__)
				// 启用 TCP_QUICKACK, 减少 ACK 延迟.
				{
					auto ret = set_tcp_quickack(socket.native_handle(), true);
					if (ret.has_error())
					{
						log_conn_warning()
							<< ", tcp quickack error: "
							<< ret.error().message();
					}
				}
#endif

				// 增大 TCP 缓冲区以提升吞吐量.
				{
					auto ret = set_tcp_buffer_sizes(
						socket.native_handle(), 256 * 1024, 256 * 1024);
					if (ret.has_error())
					{
						log_conn_warning()
							<< ", tcp buffer sizes error: "
							<< ret.error().message();
					}
				}

				break;
			}
		}

		co_return ec;
	}

	net::awaitable<variant_stream_type>
	proxy_session::instantiate_proxy_pass(tcp::socket remote_socket, bool proxy_pass_use_ssl) noexcept
	{
		boost::system::error_code ec;

		if (!proxy_pass_use_ssl)
		{
			// 非 ssl 加密, 直接使用 tcp socket 实例化成 variant_stream_type 类型的对象.
			auto sock_stream = init_proxy_stream(std::move(remote_socket));
			auto& sock = boost::variant2::get<proxy_tcp_socket>(sock_stream);

			if (!m_option.scramble_)
				co_return sock_stream;

			// 若是 scramble 混淆, 则配置相应的 key.
			sock.set_scramble_key(m_client_tx_key);
			sock.set_unscramble_key(m_client_rx_key);

			co_return sock_stream;
		}

		// 获取 proxy_pass 的主机名称.
		auto proxy_host = std::string(m_proxy_pass->host_address());

		// 若用户配置了 SNI, 则使用用户配置的 SNI 否则使用默认 hostname.
		std::string sni = m_option.proxy_ssl_name_.empty() ? proxy_host : m_option.proxy_ssl_name_;

		// 获取进程内共享的 SSL context (按 是否校验证书/SNI/自定义 CA 路径 缓存),
		// 避免每个会话都重新创建 context 并加载 CA 根证书; 该操作开销很大, 在连接
		// 频繁时会成为 CPU 热点.
		std::string ctx_tag;
		ctx_tag.reserve(sni.size() + m_option.ssl_cacert_path_.size() + 16);
		ctx_tag += "proxy_pass\n";
		ctx_tag += m_option.disable_check_cert_ ? '1' : '0';
		ctx_tag += '\n';
		ctx_tag += sni;
		ctx_tag += '\n';
		ctx_tag += m_option.ssl_cacert_path_;

		auto cli_ctx = get_cached_ssl_client_ctx(ctx_tag,
			[this, &sni](net::ssl::context& ctx) noexcept
				-> boost::system::error_code
			{
				// 使用通用函数配置 SSL context (验证模式、CA 证书、主机名验证).
				boost::system::error_code cec = configure_ssl_client_ctx(ctx,
					m_option.disable_check_cert_,
					sni,
					m_option.ssl_cacert_path_);
				if (cec)
				{
					log_conn_warning()
						<< ", configure ssl context error: " << cec.message();
				}

				// 额外的 SSL 上下文配置: DH 参数和 ALPN 协议.
				ctx.use_tmp_dh(net::buffer(default_dh_param()), cec);
				SSL_CTX_set_alpn_protos(ctx.native_handle(),
					(const unsigned char *)"\x08http/1.1", 9);

				return {};
			});

		if (cli_ctx.has_error())
		{
			log_conn_warning()
				<< ", get ssl context error: " << cli_ctx.error().message();
			co_return init_proxy_stream(std::move(remote_socket));
		}

		// 生成 ssl_tcp_stream 的 variant_stream_type 对象.
		auto sock_stream = init_proxy_stream(
			std::move(remote_socket), *cli_ctx.value());

		// get origin ssl stream type.
		ssl_tcp_stream& ssl_socket = boost::variant2::get<ssl_tcp_stream>(sock_stream);

		if (m_option.scramble_)
		{
			// 若是 scramble 混淆, 则配置相应的 key.
			auto& next_layer = ssl_socket.next_layer();

			next_layer.set_scramble_key(m_client_tx_key);
			next_layer.set_unscramble_key(m_client_rx_key);
		}

		// Set SNI Hostname.
		if (!SSL_set_tlsext_host_name(ssl_socket.native_handle(), sni.c_str()))
		{
			log_conn_warning()
				<< ", set ssl sni name: "
				<< sni
				<< " failed, error: "
				<< ::ERR_get_error();
		}

		log_conn_debug() << ", performing TLS handshake with " << sni << "...";

		// do async handshake.
		co_await ssl_socket.async_handshake(net::ssl::stream_base::client, net_awaitable[ec]);
		if (ec)
		{
			log_conn_warning()
				<< ", TLS handshake failed with " << proxy_host
				<< ", error: " << ec.message();
		}

		log_conn_debug() << ", TLS handshake with " << proxy_host << " completed successfully";

		co_return sock_stream;
	}

	net::awaitable<void> proxy_session::idle_timeout(
		variant_stream_type& s1, variant_stream_type& s2) noexcept
	{
		auto executor = co_await net::this_coro::executor;
		net::steady_timer timer(executor);
		boost::system::error_code ec;

		// tcp_timeout_ <= 0 表示禁用超时检查, 此时 idle_timeout 永不主动触发超时,
		// 只等待两个方向的传输都完成(即本协程被并发传输中的或运算取消)或连接被
		// 关闭后退出. 但若某个方向的 transfer 已因错误退出, 会把 m_transfer_timeout
		// 重置为 5 秒兜底, 避免另一方向因对端忽略 FIN 而永久挂起.

		for (; !m_abort;)
		{
			// 每次循环重新读取, 使 transfer 退出分支中动态设置的兜底超时生效.
			const int timeout = m_transfer_timeout.load(std::memory_order_relaxed);

			if (timeout > 0)
			{
				auto last_tick = m_last_activity.fetch_add(1, std::memory_order_relaxed) + 1;
				if (last_tick >= timeout)
				{
					// 没有任何方向有数据传输, 超时关闭连接.
					log_conn_warning() << ", idle timeout: " << last_tick;
					m_abort = true;
					stream_cancel(s1);
					stream_cancel(s2);
					co_return;
				}

				if (last_tick % 3600 == 0)
					log_conn_debug() << ", idle timeout long time check: " << last_tick;
			}

			timer.expires_after(std::chrono::seconds(1));
			co_await timer.async_wait(net_awaitable[ec]);

			// 当两个方向的传输都已完成时, 本协程会被并发传输中的或运算取消,
			// 此时 async_wait 会以 operation_aborted 返回, 直接退出, 避免连接
			// 已断开后 session 仍一直存活到空闲超时.
			if (ec)
				co_return;
		}

		co_return;
	}

	template <typename S1, typename S2>
	net::awaitable<void>
	proxy_session::transfer(S1& from, S2& to, size_t& bytes_transferred, bool count_rx) noexcept
	{
		bytes_transferred = 0;

		// 记录 from 和 to 的 endpoint 信息.
		auto from_endpoint = remote_endpoint_string(from);
		auto to_endpoint = remote_endpoint_string(to);

		constexpr auto buf_size = 1024 * 1024; // 1 MiB 收发缓冲

		std::unique_ptr<char, decltype(&std::free)> buf0((char*)std::malloc(buf_size), &std::free);
		std::unique_ptr<char, decltype(&std::free)> buf1((char*)std::malloc(buf_size), &std::free);

		// 分别使用主从缓冲指针用于并发读写.
		auto primary_buf = buf0.get();
		auto secondary_buf = buf1.get();

		boost::system::error_code from_ec;
		boost::system::error_code to_ec;

		// 设置传输限速.
		stream_rate_limit(from, m_option.tcp_rate_limit_);
		stream_rate_limit(to, m_option.tcp_rate_limit_);

		// 首先读取第一个数据作为预备, 以用于后面的交替读写逻辑.
		auto bytes = co_await from.async_read_some(net::buffer(primary_buf, buf_size), net_awaitable[from_ec]);
		if (from_ec || m_abort)
		{
			// WebSocket ABORT 或会话已中止时必须立即关闭双向传输, 不能向目标
			// 发送 FIN 半关闭, 否则另一方向仍可能继续转发数据.
			if (from_ec == net::error::connection_aborted || m_abort)
			{
				close();
				co_return;
			}

			if (from_ec != net::error::eof)
			{
				log_conn_warning()
					<< ", read from endpoint: " << from_endpoint
					<< ", error: " << from_ec.message();
			}

			// 发生错误, 重置超时时间, 避免另一个 transfer 因另一端忽略响应 eof 导致长期不退出.
			if (m_transfer_timeout <= 0)
				m_transfer_timeout.store(tcp_session_expired_time, std::memory_order_relaxed);

			log_conn_warning()
				<< ", shutdown to endpoint: " << to_endpoint;
			co_await async_shutdown(to, net_awaitable[to_ec]);
			co_return;
		}

		// 累计本方向流量（供 launcher 状态统计）.
		if (count_rx)
			m_total_rx += static_cast<uint64_t>(bytes);
		else
			m_total_tx += static_cast<uint64_t>(bytes);

		// 流量配额检查（上行+下行总和）: 超出配额立即断开连接.
		if (quota_exceeded())
		{
			log_conn_warning()
				<< ", traffic quota exceeded, closing connection, user: "
				<< m_auth_user;
			close();
			co_return;
		}

		// 重置最后活跃计数.
		m_last_activity.store(0, std::memory_order_relaxed);

		for (; !m_abort;)
		{
			// 并发读写, 将上次接收到的数据 primary_buf 发送给 to, 同时异步读取 from 的数
			// 据到 secondary_buf 中.
			auto [write_bytes, read_bytes] =
				co_await(
					net::async_write(to,
						net::buffer(primary_buf, bytes),
						[bytes](const boost::system::error_code& ec, std::size_t) -> std::size_t {
							return ec ? 0 : bytes;
						}, net_awaitable[to_ec])
					&&
					from.async_read_some(
						net::buffer(secondary_buf, buf_size), net_awaitable[from_ec])
				);
			(void)write_bytes;

			// 交换主从缓冲区.
			std::swap(primary_buf, secondary_buf);

			// 保存接收到的数据大小用于转发给 to 端, 以及计算整个传输数据量.
			bytes = read_bytes;
			bytes_transferred += bytes;

			// 累计本方向流量（供 launcher 状态统计）.
			if (count_rx)
				m_total_rx += static_cast<uint64_t>(bytes);
			else
				m_total_tx += static_cast<uint64_t>(bytes);

			// 流量配额检查（上行+下行总和）: 超出配额立即断开连接.
			if (quota_exceeded())
			{
				log_conn_warning()
					<< ", traffic quota exceeded, closing connection, user: "
					<< m_auth_user;
				close();
				co_return;
			}

			// 重置最后活跃计数.
			m_last_activity.store(0, std::memory_order_relaxed);

			// 如果 async_write 失败, 则也无需要再读取数据, 如果 async_read_some 失败, 则
			// 也无数据可用于写, 所以无论哪一种情况都可以直接退出.
			if (from_ec || to_ec)
			{
				if (from_ec == net::error::connection_aborted)
				{
					close();
					co_return;
				}

				if (from_ec && from_ec != net::error::eof)
				{
					log_conn_warning()
						<< ", read from endpoint: " << from_endpoint
						<< ", error: " << from_ec.message();
				}
				if (to_ec && to_ec != net::error::eof)
				{
					log_conn_warning()
						<< ", write to endpoint: " << to_endpoint
						<< ", error: " << to_ec.message();
				}

				// 发生错误, 重置超时时间, 避免另一个 transfer 因另一端忽略响应 eof 导致长期不退出.
				if (m_transfer_timeout <= 0)
					m_transfer_timeout.store(tcp_session_expired_time, std::memory_order_relaxed);

				// shutdown 当前连接, 只关闭非 TLS 连接, 而 TLS 连接需要在 transfer 完成后再
				// 操作, 否则有可能因为 async_shutdown 内部异步 io 导致未定义行业.
				if (from_ec)
				{
					log_conn_warning()
						<< ", shutdown to endpoint: " << to_endpoint;
					co_await async_shutdown(to, net_awaitable[to_ec]);
				}

				// 只要出错即可退出.
				co_return;
			}
		}

		co_return;
	}

	// explicit instantiation for transfer
	template
	net::awaitable<void>
	proxy_session::transfer<variant_stream_type, variant_stream_type>(
		variant_stream_type& from, variant_stream_type& to, size_t& bytes_transferred, bool count_rx) noexcept;

	net::awaitable<void> proxy_session::concurrent_transfer()
	{
		size_t l2r_transferred = 0;
		size_t r2l_transferred = 0;

		// 重置连接活动.
		m_last_activity.store(0, std::memory_order_relaxed);

		// 保存超时配置.
		m_transfer_timeout.store(m_option.tcp_timeout_, std::memory_order_relaxed);

		// 补发协议握手阶段被 Beast 预读到缓冲区中的数据.
		boost::system::error_code buffer_ec;
		if (m_local_buffer.size() > 0)
		{
			auto bytes = m_local_buffer.size();
			co_await net::async_write(m_remote_socket,
				m_local_buffer.data(), net_awaitable[buffer_ec]);
			m_local_buffer.consume(bytes);
			if (buffer_ec)
			{
				log_conn_warning()
					<< ", write buffered local data error: "
					<< buffer_ec.message();
				close();
				co_return;
			}

			m_total_rx += static_cast<uint64_t>(bytes);
			m_last_activity.store(0, std::memory_order_relaxed);
		}

		if (m_remote_buffer.size() > 0)
		{
			auto bytes = m_remote_buffer.size();
			co_await net::async_write(m_local_socket,
				m_remote_buffer.data(), net_awaitable[buffer_ec]);
			m_remote_buffer.consume(bytes);
			if (buffer_ec)
			{
				log_conn_warning()
					<< ", write buffered remote data error: "
					<< buffer_ec.message();
				close();
				co_return;
			}

			m_total_tx += static_cast<uint64_t>(bytes);
			m_last_activity.store(0, std::memory_order_relaxed);
		}

		// 并发读写, 在 local 和 remote 之间互传数据.
		// 同时运行 idle_timeout 检测整体连接空闲超时.
		// 只要任何一个方向有数据传输, idle_timeout 中的计时器就会被重置,
		// 从而避免单向大文件传输时因空闲方向超时而断开连接.
		// 使用或运算组合: 当两个方向的传输都完成(即两端都已断开)时立即结束,
		// 此时 idle_timeout 会被或运算取消并退出; 若一直空闲, 则由
		// idle_timeout 触发超时并关闭连接.
		// local -> remote 方向为客户端上行（计入 RX），
		// remote -> local 方向为客户端下行（计入 TX，即下载量）。
		co_await(
			(transfer(m_local_socket, m_remote_socket, l2r_transferred, true)
			 &&
			 transfer(m_remote_socket, m_local_socket, r2l_transferred, false))
			||
			idle_timeout(m_local_socket, m_remote_socket)
		);

		log_conn_debug()
			<< ", transfer completed"
			<< ", local to remote: "
			<< l2r_transferred
			<< ", remote to local: "
			<< r2l_transferred;

		co_return;
	}

} // namespace proxy
