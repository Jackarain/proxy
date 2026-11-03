//
// doh_proxy_pass_pool.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/doh_client.hpp"
#include "proxy/authority.hpp"
#include "proxy/proxy_websocket_client.hpp"

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <cerrno>
#include <algorithm>
#include <utility>

#if !defined(_WIN32)
# include <sys/socket.h>
#endif

namespace proxy {

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

			std::string proxy_host(proxy_url.host_address());
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

		std::string proxy_host(proxy_url.host_address());
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

		// 建连（含 protect/多地址重试）总超时, 黑洞网络下可能悬挂:
		// 超时后关闭在途 socket 使 async_connect 立即失败并退出.
		net::steady_timer conn_timer(executor);
		auto current = std::make_shared<tcp::socket>(executor);
		bool timed_out = false;
		conn_timer.expires_after(std::chrono::seconds(15));
		conn_timer.async_wait(
			[current, &timed_out](const boost::system::error_code& tec)
			{
				if (tec)
					return;
				timed_out = true;
				boost::system::error_code sec;
				current->close(sec);
			});

		for (const auto& t : targets)
		{
			if (timed_out)
				break;

			boost::system::error_code sec;
			current->close(sec);
			current->open(t.endpoint().protocol(), ec);
			if (ec)
				continue;

			// 设置 SO_MARK（配合策略路由排除代理自身流量，防止环路）.
			apply_so_mark_if(*current, opt);

			// 先放行再 connect，防止 SYN 回环进 tun 形成环路.
			if (protect && !co_await protect(current->native_handle()))
				continue;

			if (timed_out)
				break;

			co_await current->async_connect(t.endpoint(), net_awaitable[ec]);
			if (!ec)
			{
				conn_timer.cancel();
				co_return std::move(*current);
			}
		}

		conn_timer.cancel();
		XLOG_WARN << "tun connect proxy_pass: " << proxy_host
			<< ", error: " << ec.message();
		co_return tcp::socket(executor);
	}

} // namespace proxy
