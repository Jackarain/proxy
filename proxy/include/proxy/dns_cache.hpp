//
// dns_cache.hpp
// ~~~~~~~~~~~~~
//
// Copyright (c) 2025 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef INCLUDE__2025_11_25__DNS_CACHE_HPP
#define INCLUDE__2025_11_25__DNS_CACHE_HPP

#include <string>
#include <tuple>
#include <chrono>
#include <list>
#include <unordered_map>

#include <boost/asio/ip/basic_resolver.hpp>

namespace proxy {

	namespace net = boost::asio;
	using tcp = net::ip::tcp;

	using dns_result =
		std::tuple<
			tcp::resolver::results_type,			// dns results
			std::chrono::steady_clock::time_point	// for expire
		>;

	class dns_cache
	{
		dns_cache(const dns_cache&) = delete;
		dns_cache& operator=(const dns_cache&) = delete;

	public:
		using key_type = std::string;
		using value_type = dns_result;
		using list_value = dns_result;
		using list_type = std::list<std::pair<key_type, list_value>>;

		const std::chrono::seconds expire_time = std::chrono::seconds(300);

	public:
		explicit dns_cache(size_t max_items)
			: m_capacity(max_items)
		{
			if (max_items == 0)
				throw std::invalid_argument("capacity must > 0");
		}

		inline size_t size() const noexcept
		{
			return m_cache_map.size();
		}

		inline size_t max_size() const noexcept
		{
			return m_capacity;
		}

		inline std::optional<dns_result> get(const key_type& key)
		{
			auto it = m_cache_map.find(key);
			if (it == m_cache_map.end())
				return {};

			// 检查 dns 是否超时.
			auto [result, tp] = it->second->second;

			auto now = std::chrono::steady_clock::now();
			if (tp + expire_time < now)
				return {};

			// splice 把节点移动到链表头部(把最近使用的提前)
			m_lru_list.splice(m_lru_list.begin(), m_lru_list, it->second);

			// 更新 map 中的迭代器.
			it->second = m_lru_list.begin();
			// 更新时间点.
			it->second->second = std::make_tuple(std::get<0>(it->second->second), now);

			// 返回 dns_result
			return it->second->second;
		}

		inline void put(const key_type& key, value_type&& results)
		{
			auto it = m_cache_map.find(key);
			if (it != m_cache_map.end())
			{
				// 已存在：更新并移到最前面.
				it->second->second = results;
				m_lru_list.splice(m_lru_list.begin(), m_lru_list, it->second);
				it->second = m_lru_list.begin();
				return;
			}

			// 超出容量, 淘汰最久未使用的.
			if (m_cache_map.size() >= m_capacity)
			{
				auto& node = m_lru_list.back();
				m_cache_map.erase(node.first);
				m_lru_list.pop_back();
			}

			// 新条目插到最前面.
			m_lru_list.emplace_front(std::move(key), std::move(results));
			m_cache_map[m_lru_list.front().first] = m_lru_list.begin();
		}

		inline void put(const key_type& key, const value_type& results)
		{
			put(key, value_type{ results }); // 复制一次
		}

	private:
		list_type m_lru_list;
		std::unordered_map<key_type, list_type::iterator> m_cache_map;
		const size_t m_capacity;
	};
}

#endif // INCLUDE__2025_11_25__DNS_CACHE_HPP
