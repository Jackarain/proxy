//
// proxy_session_transfer.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~
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


	net::awaitable<void> proxy_session::idle_timeout(
		variant_stream_type& s1, variant_stream_type& s2) noexcept
	{
		co_await idle_timeout([this, &s1, &s2]()
		{
			stream_cancel(s1);
			stream_cancel(s2);
		});
	}

	net::awaitable<void> proxy_session::idle_timeout(
		std::function<void()> cancel) noexcept
	{
		auto executor = co_await net::this_coro::executor;
		net::steady_timer timer(executor);
		boost::system::error_code ec;

		// 超时值 <= 0 表示禁用超时检查, 此时 idle_timeout 永不主动触发超时,
		// 只等待调用方取消或连接关闭后退出.

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
					cancel();
					co_return;
				}

				if (last_tick % 3600 == 0)
					log_conn_debug() << ", idle timeout long time check: " << last_tick;
			}

			timer.expires_after(std::chrono::seconds(1));
			co_await timer.async_wait(net_awaitable[ec]);

			// 当传输完成或连接关闭时, 本协程会被并发传输中的或运算取消,
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
