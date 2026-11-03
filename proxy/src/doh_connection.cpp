//
// doh_connection.cpp
// ~~~~~~~~~~~~~~~~~~
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

#include <utility>

namespace proxy {

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
		const bool proxy_uses_ssl = proxy_use_ssl(proxy_url, m_option);
		m_doh_host = std::string(proxy_url.host_address());
		m_doh_port = proxy_url.port_number();
		if (m_doh_port == 0)
			m_doh_port = urls::default_port(proxy_url.scheme_id());
		if (m_doh_port == 0)
			m_doh_port = 443;
		m_doh_https = proxy_uses_ssl;
		m_doh_path = "/dns-query";

		if (!m_option.dns_doh_.empty())
		{
			if (auto r = urls::parse_uri(m_option.dns_doh_); r.has_value())
			{
				m_doh_host = std::string(r->host_address());
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
			std::string(proxy_url.host_address()));
		auto proxy_port = proxy_url.port_number();
		if (proxy_port == 0)
			proxy_port = proxy_pass_default_port(proxy_url);
		auto doh_host = boost::to_lower_copy(m_doh_host);
		m_doh_via_proxy = !doh_host.empty() &&
			(doh_host != proxy_host || m_doh_port != proxy_port ||
				m_doh_https != proxy_uses_ssl);
		m_doh_authority = authority::format_authority(m_doh_host, m_doh_port);

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
		bool ok = co_await doh_post(*m_stream, m_doh_authority, m_doh_path,
			dns_query, output,
			m_doh_via_proxy ? std::string{} : m_auth_user,
			m_doh_via_proxy ? std::string{} : m_auth_pass);

		m_req_timer.cancel();
		co_return ok;
	}

} // namespace proxy
