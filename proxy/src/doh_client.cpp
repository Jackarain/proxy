//
// doh_client.cpp
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/doh_client.hpp"

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <cerrno>
#include <algorithm>
#include <utility>

#if !defined(_WIN32)
# include <sys/socket.h>
#endif

namespace proxy {

	//////////////////////////////////////////////////////////////////////////
	// proxy_pass_pool

	proxy_pass_pool::proxy_pass_pool(net::any_io_executor executor,
		proxy_server_option opt,
		std::function<net::awaitable<bool>(int)> protect,
		size_t target)
		: m_executor(std::move(executor))
		, m_option(std::move(opt))
		, m_protect(std::move(protect))
		, m_target(target)
	{
		m_timer.emplace(m_executor);
	}

	proxy_pass_pool::~proxy_pass_pool()
	{
		close();
	}

	void proxy_pass_pool::start()
	{
		if (m_target == 0 || m_closed)
			return;

		if (m_maintaining)
			return;
		m_maintaining = true;

		net::co_spawn(m_executor,
			[self = shared_from_this()]() -> net::awaitable<void>
			{
				co_await self->maintain();
			}, net::detached);
	}

	void proxy_pass_pool::close()
	{
		if (m_closed)
			return;
		m_closed = true;

		std::deque<variant_stream_type> idle;
		{
			std::lock_guard<std::mutex> lk(m_mutex);
			idle.swap(m_idle);
		}
		for (auto& s : idle)
		{
			boost::system::error_code ec;
			s.close(ec);
		}

		if (m_timer)
			m_timer->cancel();
	}

	net::awaitable<std::optional<variant_stream_type>>
	proxy_pass_pool::acquire()
	{
		{
			std::lock_guard<std::mutex> lk(m_mutex);
			if (!m_idle.empty())
			{
				auto s = std::move(m_idle.front());
				m_idle.pop_front();
				// 唤醒维护协程立即补充空闲连接.
				if (m_timer)
					m_timer->cancel();
				co_return s;
			}
		}

		// 池空：直接新建一条（避免等待维护协程补充的延迟）.
		auto stream = co_await make_connection();
		if (stream.is_open())
			co_return std::move(stream);
		co_return std::nullopt;
	}

	net::awaitable<void> proxy_pass_pool::maintain()
	{
		auto last_rebuild = std::chrono::steady_clock::now();

		for (;;)
		{
			if (m_closed)
			{
				m_maintaining = false;
				co_return;
			}

			// 健康检查：剔除已被对端断开的空闲连接.
			prune_dead();

			// 补充连接：池未满立即连续建连直到补满
			// （启动预热与取走/断开后的补充均不限速）.
			if (idle_count() < m_target)
			{
				bool filled = true;
				while (idle_count() < m_target && !m_closed)
				{
					if (!co_await build_one())
					{
						filled = false;
						break;
					}
				}
				if (m_closed)
				{
					m_maintaining = false;
					co_return;
				}
				if (filled)
				{
					m_reached_target = true;
					continue;  // 已补满，回到循环.
				}
				// 建连失败：落到下方等待 k_retry_interval 后重试.
			}

			// 保活：空闲连接超过 k_idle_timeout 未被使用，整体重建.
			auto now = std::chrono::steady_clock::now();
			if (m_reached_target && idle_count() >= m_target &&
				now - last_rebuild >= k_idle_timeout)
			{
				std::deque<variant_stream_type> stale;
				{
					std::lock_guard<std::mutex> lk(m_mutex);
					stale.swap(m_idle);
				}
				for (auto& s : stale)
				{
					boost::system::error_code ec;
					s.close(ec);
				}
				last_rebuild = now;
				continue;  // 重建后回到循环立即补齐.
			}

			// 等待：建连失败后的重试间隔，或被 acquire 取走连接时
			// 取消定时器立即唤醒补充.
			m_timer->expires_after(k_retry_interval);
			boost::system::error_code ec;
			co_await m_timer->async_wait(net_awaitable[ec]);
			if (m_closed)
			{
				m_maintaining = false;
				co_return;
			}
			// operation_aborted 表示有连接被取走，立即补充.
		}
	}

	net::awaitable<bool> proxy_pass_pool::build_one()
	{
		auto stream = co_await make_connection();
		if (!stream.is_open())
			co_return false;

		{
			std::lock_guard<std::mutex> lk(m_mutex);
			if (m_closed)
			{
				boost::system::error_code ec;
				stream.close(ec);
				co_return false;
			}
			m_idle.push_back(std::move(stream));
		}
		co_return true;
	}

	size_t proxy_pass_pool::idle_count() const noexcept
	{
		std::lock_guard<std::mutex> lk(m_mutex);
		return m_idle.size();
	}

	void proxy_pass_pool::prune_dead()
	{
		std::lock_guard<std::mutex> lk(m_mutex);
		for (auto it = m_idle.begin(); it != m_idle.end(); )
		{
			if (!is_conn_alive(*it))
			{
				boost::system::error_code ec;
				it->close(ec);
				it = m_idle.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	bool proxy_pass_pool::is_conn_alive(
		variant_stream_type& stream) noexcept
	{
		if (!stream.is_open())
			return false;

		tcp::socket& sock = net_tcp_socket(stream);
		int fd = sock.native_handle();
		if (fd < 0)
			return false;

#if defined(_WIN32)
		// Windows 上不做非阻塞探测，依赖 k_idle_timeout 周期重建保活.
		(void)fd;
		return true;
#else
		// 非阻塞 peek 探测对端是否已断开：
		// - 返回 0 表示对端已 FIN（EOF）；
		// - EAGAIN/EWOULDBLOCK 表示连接正常，无数据可读；
		// - 其他错误表示连接已断开.
		// 注意：有数据可读不代表连接异常——TLS 会话中服务端可能主动
		// 发送 NewSessionTicket、HTTP/2 设置帧等（PEEK 只能看到密文），
		// 若据此丢弃会导致池连接被反复误剔，故有数据时判定连接存活.
		char buf;
		ssize_t n = ::recv(fd, &buf, 1, MSG_PEEK | MSG_DONTWAIT);
		if (n == 0)
			return false;
		if (n < 0)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				return true;
			return false;
		}
		return true;
#endif
	}

	net::awaitable<variant_stream_type>
	proxy_pass_pool::make_connection()
	{
		const auto& proxy_url = *m_option.proxy_pass_;

		auto sock = co_await connect_proxy_pass(
			m_executor, m_option, proxy_url, m_protect);
		if (!sock.is_open())
			co_return init_proxy_stream(m_executor);

		// SSL 代理（https/wss 或显式配置 proxy_pass_ssl）需要先完成 TLS 握手.
		if (proxy_use_ssl(proxy_url, m_option))
		{
			boost::system::error_code ec;

			std::string proxy_host(proxy_url.encoded_host());
			const std::string sni = m_option.proxy_ssl_name_.empty() ?
				proxy_host : m_option.proxy_ssl_name_;

			if (!m_ssl_ctx)
			{
				m_ssl_ctx.emplace(net::ssl::context::sslv23_client);
				ec = configure_ssl_client_ctx(*m_ssl_ctx,
					m_option.disable_check_cert_, sni,
					m_option.ssl_cacert_path_);
				if (ec)
				{
					m_ssl_ctx.reset();
					co_return init_proxy_stream(m_executor);
				}
			}

			auto stream = init_proxy_stream(std::move(sock), *m_ssl_ctx);
			auto& ssl_stream = boost::variant2::get<ssl_tcp_stream>(stream);
			SSL_set_tlsext_host_name(
				ssl_stream.native_handle(), sni.c_str());

			co_await ssl_stream.async_handshake(
				net::ssl::stream_base::client, net_awaitable[ec]);
			if (ec)
				co_return init_proxy_stream(m_executor);

			co_return stream;
		}

		co_return init_proxy_stream(std::move(sock));
	}


	//////////////////////////////////////////////////////////////////////////
	// connect_proxy_pass

	net::awaitable<tcp::socket> connect_proxy_pass(
		net::any_io_executor executor,
		const proxy_server_option& opt,
		const urls::url& proxy_url,
		const std::function<net::awaitable<bool>(int)>& protect)
	{
		boost::system::error_code ec;

		std::string proxy_host(proxy_url.encoded_host());
		uint16_t proxy_port = proxy_url.port_number();
		if (proxy_port == 0)
			proxy_port = proxy_pass_default_port(proxy_url);

		tcp::resolver resolver(executor);
		auto targets = co_await resolver.async_resolve(
			proxy_host, std::to_string(proxy_port), net_awaitable[ec]);
		if (ec || targets.empty())
		{
			XLOG_WARN << "tun resolve proxy_pass: " << proxy_host
				<< ", error: " << ec.message();
			co_return tcp::socket(executor);
		}

		for (const auto& t : targets)
		{
			tcp::socket sock(executor);
			sock.open(t.endpoint().protocol(), ec);
			if (ec)
				continue;

			// 设置 SO_MARK（配合策略路由排除代理自身流量，防止环路）.
			apply_so_mark_if(sock, opt);

			// 先放行再 connect，防止 SYN 回环进 tun 形成环路.
			if (protect && !co_await protect(sock.native_handle()))
				continue;

			co_await sock.async_connect(t.endpoint(), net_awaitable[ec]);
			if (!ec)
				co_return sock;
		}

		XLOG_WARN << "tun connect proxy_pass: " << proxy_host
			<< ", error: " << ec.message();
		co_return tcp::socket(executor);
	}

	//////////////////////////////////////////////////////////////////////////
	// doh_connection

	doh_connection::doh_connection(net::any_io_executor executor,
		proxy_server_option opt,
		std::function<net::awaitable<bool>(int)> protect,
		std::shared_ptr<proxy_pass_pool> proxy_pool)
		: m_executor(std::move(executor))
		, m_option(std::move(opt))
		, m_protect(std::move(protect))
		, m_proxy_pool(std::move(proxy_pool))
		, m_idle_timer(m_executor)
		, m_req_timer(m_executor)
		, m_conn_timer(m_executor)
	{
		// 解析 DoH 目标（未配置 dns_doh_ 时把 proxy_pass 当作 DoH 服务）.
		const auto& proxy_url = *m_option.proxy_pass_;
		m_doh_host = std::string(proxy_url.encoded_host());
		m_doh_port = proxy_url.port_number();
		if (m_doh_port == 0)
			m_doh_port = urls::default_port(proxy_url.scheme_id());
		if (m_doh_port == 0)
			m_doh_port = 443;
		m_doh_path = "/dns-query";

		if (!m_option.dns_doh_.empty())
		{
			if (auto r = urls::parse_uri(m_option.dns_doh_); r.has_value())
			{
				m_doh_host = std::string(r->encoded_host());
				if (!r->path().empty())
					m_doh_path = std::string(r->path());
				m_doh_port = r->port_number();
				if (m_doh_port == 0)
					m_doh_port = urls::default_port(r->scheme_id());
				if (m_doh_port == 0)
					m_doh_port = 443;
				auto scheme = boost::to_lower_copy(std::string(r->scheme()));
				m_doh_https = scheme.ends_with("s");
			}
		}

		// 与 proxy_pass 同服务为直连 DoH，否则经代理 CONNECT 隧道转发.
		auto proxy_host = boost::to_lower_copy(
			std::string(proxy_url.encoded_host()));
		auto doh_host = boost::to_lower_copy(m_doh_host);
		m_doh_via_proxy = !doh_host.empty() && doh_host != proxy_host;

		m_auth_user = std::string(proxy_url.user());
		m_auth_pass = std::string(proxy_url.password());
	}

	doh_connection::~doh_connection()
	{
		close();
	}

	void doh_connection::submit(std::string dns_query,
		std::function<void(const boost::system::error_code&, std::string)> handler)
	{
		if (m_closed || m_queue.size() >= k_max_pending)
		{
			if (handler)
				handler(make_error_code(
					boost::system::errc::connection_aborted), {});
			return;
		}

		m_queue.push_back({ std::move(dns_query), std::move(handler) });
		m_idle_timer.cancel();

		// 首次提交启动 pump 协程（空闲退出后由后续 submit 重启）.
		if (!m_pumping)
		{
			m_pumping = true;
			net::co_spawn(m_executor,
				[self = shared_from_this()]() -> net::awaitable<void>
				{
					co_await self->pump();
				}, net::detached);
		}
	}

	size_t doh_connection::pending() const noexcept
	{
		return m_queue.size() + (m_busy ? 1 : 0);
	}

	void doh_connection::close()
	{
		if (m_closed)
			return;
		m_closed = true;

		m_idle_timer.cancel();
		m_req_timer.cancel();
		m_conn_timer.cancel();
		// 只中止在途操作, 不销毁流对象: pump 协程可能正挂在该流的
		// 异步读写上, 销毁动作交由 pump 在操作完成后执行.
		abort();

		// 唤醒所有排队请求（失败）.
		auto queue = std::move(m_queue);
		m_queue.clear();
		const auto ec = make_error_code(
			boost::system::errc::connection_aborted);
		for (auto& req : queue)
		{
			if (req.handler)
				req.handler(ec, {});
		}
	}

	void doh_connection::teardown()
	{
		abort();
		m_stream.reset();
	}

	void doh_connection::abort()
	{
		if (m_stream)
		{
			boost::system::error_code ec;
			m_stream->close(ec);
		}
	}

	net::awaitable<void> doh_connection::pump()
	{
		for (;;)
		{
			if (m_closed)
			{
				m_pumping = false;
				co_return;
			}

			if (m_queue.empty())
			{
				// 空闲：等待新请求或超时关闭.
				m_idle_timer.expires_after(k_idle_timeout);
				boost::system::error_code ec;
				co_await m_idle_timer.async_wait(net_awaitable[ec]);
				if (ec != net::error::operation_aborted)
				{
					// 空闲超时，关闭连接释放资源.
					m_pumping = false;
					close();
					co_return;
				}
				// 被唤醒：新请求到达或连接被关闭.
				if (m_closed)
				{
					m_pumping = false;
					co_return;
				}
				continue;
			}

			// 取出队首请求串行处理.
			auto req = std::move(m_queue.front());
			m_queue.pop_front();
			m_busy = true;

			std::string response;
			bool ok = false;
			for (int attempt = 0; attempt < 2 && !m_closed; ++attempt)
			{
				if (!m_stream && !co_await connect())
					break;  // 建连失败，重试无意义.
				if (m_closed)
					break;
				ok = co_await exchange(req.dns_query, response);
				if (ok)
					break;
				// 连接损坏，关闭后重建连接重试一次.
				teardown();
			}
			m_busy = false;

			if (req.handler)
			{
				if (ok)
					req.handler({}, std::move(response));
				else
					req.handler(make_error_code(
						boost::system::errc::connection_aborted), {});
			}
		}
	}

	net::awaitable<bool> doh_connection::connect()
	{
		// 建连整体超时: 黑洞网络下（对端无响应）TCP/TLS/隧道任一步都
		// 可能无限挂起, 超时后 abort() 关闭 m_stream 使在途建连操作
		// 立即失败, 避免 pump 与排队请求被永久悬挂.
		m_connect_aborted = false;
		const auto seq = ++m_conn_seq;
		m_conn_timer.expires_after(k_connect_timeout);
		std::weak_ptr<doh_connection> weak = shared_from_this();
		m_conn_timer.async_wait(
			[weak, seq](const boost::system::error_code& ec)
			{
				if (ec)
					return;
				if (auto self = weak.lock();
					self && !self->m_closed && seq == self->m_conn_seq)
				{
					self->m_connect_aborted = true;
					self->abort();
				}
			});

		bool ok = co_await connect_impl();

		// 使已排队（cancel 无法撤回）的超时回调过期, 避免其在后续
		// 请求阶段误中止新连接.
		++m_conn_seq;
		m_conn_timer.cancel();
		co_return ok;
	}

	net::awaitable<bool> doh_connection::connect_impl()
	{
		// 优先复用 proxy_pass 预选连接池中已建立（TCP/TLS 已完成）的连接.
		if (m_proxy_pool)
		{
			// 仅在确有现成空闲连接时复用: 空闲连接可立即取走, 不会等待
			// 池维护协程新建连接（该等待不受本连接超时控制, 可能悬挂）.
			if (m_proxy_pool->idle_count() > 0)
			{
				auto conn = co_await m_proxy_pool->acquire();
				// 超时回调可能在 acquire 等待期间触发: 此时 m_stream 尚为
				// 空, abort() 无法中止任何在途操作, 故取到连接后须再检查
				// 中止标记, 避免超时后仍继续建立隧道/握手.
				if (m_closed || m_connect_aborted)
				{
					if (conn && conn->is_open())
					{
						boost::system::error_code ec;
						conn->close(ec);
					}
					co_return false;
				}
				if (conn && conn->is_open())
				{
					if (co_await connect_pooled(std::move(*conn)))
						co_return true;

					// 池中连接不可用（代理端已断开等），回退新建连接.
					XLOG_WARN << "tun doh pool connection failed, "
						"fallback connect";
				}
			}
		}

		// 新建到代理的 TCP 连接（流挂在 m_stream 上, 可被 abort 中止）.
		if (!co_await connect_tcp_proxy())
			co_return false;

		// 经代理 CONNECT 隧道转发，或与代理同服务直连.
		co_return m_doh_via_proxy ?
			co_await connect_via_proxy() :
			co_await connect_direct();
	}

	// connect_tcp_proxy 新建到代理的 TCP 连接.
	net::awaitable<bool> doh_connection::connect_tcp_proxy()
	{
		boost::system::error_code ec;
		const auto& proxy_url = *m_option.proxy_pass_;
		std::string proxy_host(proxy_url.encoded_host());
		uint16_t proxy_port = proxy_url.port_number();
		if (proxy_port == 0)
			proxy_port = proxy_pass_default_port(proxy_url);

		tcp::resolver resolver(m_executor);
		auto targets = co_await resolver.async_resolve(
			proxy_host, std::to_string(proxy_port), net_awaitable[ec]);
		if (ec || targets.empty())
		{
			XLOG_WARN << "tun resolve proxy_pass: " << proxy_host
				<< ", error: " << ec.message();
			co_return false;
		}

		for (const auto& t : targets)
		{
			// 建连被中止（超时）或连接已关闭: 不再尝试剩余地址.
			if (m_closed || m_connect_aborted)
				co_return false;

			// 连接中的流挂在 m_stream 上: abort()（建连超时/连接关闭）
			// 可随时关闭底层 socket 中止这次尝试, 不会残留悬挂的连接.
			m_stream.emplace(init_proxy_stream(m_executor));
			auto& sock = net_tcp_socket(*m_stream);
			sock.open(t.endpoint().protocol(), ec);
			if (ec)
			{
				m_stream.reset();
				continue;
			}

			// 设置 SO_MARK（配合策略路由排除代理自身流量，防止环路）.
			apply_so_mark_if(sock, m_option);

			// 先放行再 connect，防止 SYN 回环进 tun 形成环路.
			if (m_protect && !co_await m_protect(sock.native_handle()))
			{
				m_stream.reset();
				continue;
			}

			co_await sock.async_connect(t.endpoint(), net_awaitable[ec]);
			if (!ec)
				co_return true;
			m_stream.reset();
		}

		XLOG_WARN << "tun connect proxy_pass: " << proxy_host
			<< ", error: " << ec.message();
		co_return false;
	}

	// connect_pooled 基于连接池获取的已有连接建立 DoH 连接：
	// 池连接的外层 TLS（proxy 为 https 时）已握手完成，直接复用；
	// 经代理 CONNECT 隧道时在其上建隧道，隧道内按需建内层 DoH TLS.
	net::awaitable<bool> doh_connection::connect_pooled(
		variant_stream_type stream)
	{
		const auto& proxy_url = *m_option.proxy_pass_;

		// 池连接（TCP/TLS 已完成）先挂到 m_stream: 隧道建立过程也受
		// abort()（建连超时/关闭）控制.
		m_stream = std::move(stream);

		if (!m_doh_via_proxy)
			co_return true;  // 同服务: 池连接即为 DoH 连接.

		// 经代理 CONNECT 隧道转发到 DoH 服务.
		const std::string authority =
			m_doh_host + ":" + std::to_string(m_doh_port);
		if (!co_await http_connect_tunnel(
			*m_stream, authority, proxy_url))
		{
			m_stream.reset();
			co_return false;
		}

		// 取出隧道底层 TCP socket 供内层 TLS 复用.
		tcp::socket tunnel = std::move(net_tcp_socket(*m_stream));
		m_stream.reset();

		if (!m_doh_https)
		{
			m_stream.emplace(init_proxy_stream(std::move(tunnel)));
			co_return true;
		}
		if (!co_await setup_tls(m_doh_host))
			co_return false;
		m_stream.emplace(init_proxy_stream(std::move(tunnel), *m_ssl_ctx));
		co_return co_await tls_handshake(m_doh_host);
	}

	// connect_via_proxy 经代理 CONNECT 隧道转发到 DoH 服务：
	// https 代理先与代理建立外层 TLS，再建立 CONNECT 隧道，
	// 隧道内按需与 DoH 服务建立内层 TLS.
	net::awaitable<bool> doh_connection::connect_via_proxy()
	{
		const auto& proxy_url = *m_option.proxy_pass_;
		const std::string proxy_host(proxy_url.encoded_host());
		const std::string authority =
			m_doh_host + ":" + std::to_string(m_doh_port);
		const std::string proxy_sni = m_option.proxy_ssl_name_.empty() ?
			proxy_host : m_option.proxy_ssl_name_;

		// https 代理先与代理建立外层 TLS（流由 m_stream 持有, 可被中止）.
		if (proxy_use_ssl(proxy_url, m_option))
		{
			if (!m_proxy_ssl_ctx)
			{
				m_proxy_ssl_ctx.emplace(net::ssl::context::sslv23_client);
				boost::system::error_code ec;
				ec = configure_ssl_client_ctx(*m_proxy_ssl_ctx,
					m_option.disable_check_cert_, proxy_sni,
					m_option.ssl_cacert_path_);
				if (ec)
				{
					m_proxy_ssl_ctx.reset();
					m_stream.reset();
					co_return false;
				}
			}

			// 把明文 TCP 流升级为外层 TLS 流（当前无在途操作, 安全重建）.
			tcp::socket raw = std::move(net_tcp_socket(*m_stream));
			m_stream.reset();
			m_stream.emplace(init_proxy_stream(std::move(raw), *m_proxy_ssl_ctx));
			if (!co_await tls_handshake(proxy_sni))
				co_return false;
		}

		// 建立 CONNECT 隧道.
		if (!co_await http_connect_tunnel(
			*m_stream, authority, proxy_url))
		{
			m_stream.reset();
			co_return false;
		}

		// 取出隧道底层 TCP socket 供内层 TLS 复用.
		tcp::socket tunnel = std::move(net_tcp_socket(*m_stream));
		m_stream.reset();

		if (!m_doh_https)
		{
			m_stream.emplace(init_proxy_stream(std::move(tunnel)));
			co_return true;
		}

		if (!co_await setup_tls(m_doh_host))
			co_return false;
		m_stream.emplace(init_proxy_stream(std::move(tunnel), *m_ssl_ctx));
		co_return co_await tls_handshake(m_doh_host);
	}

	// connect_direct 直连 DoH 服务：与 proxy_pass 同服务（或未配置
	// dns_doh_ 时把 proxy_pass 当作 DoH 服务器），https 时走 TLS.
	net::awaitable<bool> doh_connection::connect_direct()
	{
		const auto& proxy_url = *m_option.proxy_pass_;

		if (!proxy_use_ssl(proxy_url, m_option))
			co_return true;  // 明文直连: m_stream 已是连接好的明文流.

		const std::string proxy_host(proxy_url.encoded_host());
		const std::string sni = m_option.proxy_ssl_name_.empty() ?
			proxy_host : m_option.proxy_ssl_name_;

		if (!co_await setup_tls(sni))
		{
			m_stream.reset();
			co_return false;
		}

		// 把明文 TCP 流升级为 TLS 流（当前无在途操作, 安全重建）.
		tcp::socket raw = std::move(net_tcp_socket(*m_stream));
		m_stream.reset();
		m_stream.emplace(init_proxy_stream(std::move(raw), *m_ssl_ctx));
		co_return co_await tls_handshake(sni);
	}

	// setup_tls 惰性初始化 SSL context，并以 verify_host 配置证书校验与 SNI.
	net::awaitable<bool> doh_connection::setup_tls(
		const std::string& verify_host)
	{
		boost::system::error_code ec;

		if (!m_ssl_ctx)
		{
			m_ssl_ctx.emplace(net::ssl::context::sslv23_client);
			m_ssl_configured = false;
		}
		if (!m_ssl_configured)
		{
			ec = configure_ssl_client_ctx(*m_ssl_ctx,
				m_option.disable_check_cert_, verify_host,
				m_option.ssl_cacert_path_);
			if (ec)
				co_return false;
			m_ssl_configured = true;
		}
		co_return true;
	}

	// tls_handshake 对 m_stream 中的 SSL 流设置 SNI 并完成客户端握手，
	// 失败时销毁流后返回 false.
	net::awaitable<bool> doh_connection::tls_handshake(
		const std::string& host)
	{
		boost::system::error_code ec;
		auto& ssl = boost::variant2::get<ssl_tcp_stream>(*m_stream);
		SSL_set_tlsext_host_name(ssl.native_handle(), host.c_str());
		co_await ssl.async_handshake(
			net::ssl::stream_base::client, net_awaitable[ec]);
		if (ec)
		{
			m_stream.reset();
			co_return false;
		}
		co_return true;
	}

	net::awaitable<bool> doh_connection::exchange(
		const std::string& dns_query, std::string& output)
	{
		if (!m_stream)
			co_return false;

		// 单次请求超时：超时后仅关闭底层连接使挂起的读写立即失败,
		// 不得在此销毁流对象（pump 协程正挂在该流的异步读上, asio
		// 禁止在异步操作未完成时销毁 I/O 对象, 销毁 ssl 流会访问已
		// 释放的 engine/BIO, 导致 io 线程单 handler 内无限自旋）.
		// 用 weak_ptr 防止连接析构后 handler 访问已销毁对象.
		const auto seq = ++m_req_seq;
		m_req_timer.expires_after(k_request_timeout);
		std::weak_ptr<doh_connection> weak = shared_from_this();
		m_req_timer.async_wait(
			[weak, seq](const boost::system::error_code& ec)
			{
				if (ec)
					return;
				if (auto self = weak.lock();
					self && !self->m_closed && seq == self->m_req_seq)
					self->abort();
			});

		// 经代理隧道转发时认证头仅用于代理 CONNECT（由 http_connect_tunnel
		// 发送），不向 DoH 服务器附带；直连同服务时透传 proxy 的 Basic 认证.
		bool ok = co_await doh_post(*m_stream, m_doh_host, m_doh_path,
			dns_query, output,
			m_doh_via_proxy ? std::string{} : m_auth_user,
			m_doh_via_proxy ? std::string{} : m_auth_pass);

		m_req_timer.cancel();
		co_return ok;
	}

	//////////////////////////////////////////////////////////////////////////
	// doh_client

	doh_client::doh_client(net::any_io_executor executor,
		proxy_server_option opt,
		std::function<net::awaitable<bool>(int)> protect,
		std::shared_ptr<proxy_pass_pool> proxy_pool)
		: m_executor(std::move(executor))
		, m_option(std::move(opt))
		, m_protect(std::move(protect))
		, m_proxy_pool(std::move(proxy_pool))
	{}

	net::awaitable<std::string> doh_client::query(
		const std::string& dns_query)
	{
		struct state
		{
			boost::system::error_code ec;
			std::string result;
			bool ready { false };
			net::steady_timer timer;
			explicit state(net::any_io_executor ex)
				: timer(ex)
			{
				// 挂起到 handler 回调：期满前 cancel 唤醒即可退出.
				timer.expires_at(
					std::chrono::steady_clock::time_point::max());
			}
		};

		auto st = std::make_shared<state>(m_executor);

		pick()->submit(dns_query,
			[st](const boost::system::error_code& ec, std::string result)
			{
				st->ec = ec;
				st->result = std::move(result);
				st->ready = true;
				st->timer.cancel();
			});

		while (!st->ready)
		{
			boost::system::error_code ec;
			co_await st->timer.async_wait(net_awaitable[ec]);
		}

		co_return std::move(st->result);
	}

	void doh_client::close()
	{
		for (auto& conn : m_conns)
			conn->close();
		m_conns.clear();
	}

	std::shared_ptr<doh_connection> doh_client::pick()
	{
		// 移除已关闭的连接.
		m_conns.erase(std::remove_if(m_conns.begin(), m_conns.end(),
			[](const auto& c) { return c->closed(); }), m_conns.end());

		// 优先复用空闲连接.
		for (auto& c : m_conns)
			if (c->pending() == 0)
				return c;

		// 未达上限时新建连接.
		if (m_conns.size() < k_max_connections)
		{
			auto conn = std::make_shared<doh_connection>(
				m_executor, m_option, m_protect, m_proxy_pool);
			m_conns.push_back(conn);
			return conn;
		}

		// 已满：选择排队最少的连接.
		auto it = std::min_element(m_conns.begin(), m_conns.end(),
			[](const auto& a, const auto& b)
			{
				return a->pending() < b->pending();
			});
		return *it;
	}

} // namespace proxy
