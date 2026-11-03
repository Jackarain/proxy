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

#include <algorithm>
#include <utility>

namespace proxy {

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
