//
// dns_response_cache.hpp
// ~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef INCLUDE__2026_08_10__DNS_RESPONSE_CACHE_HPP
#define INCLUDE__2026_08_10__DNS_RESPONSE_CACHE_HPP


#include <chrono>
#include <list>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>


namespace proxy {

	// dns_response_cache 用于缓存 DNS 查询结果（wire-format 响应报文）。
	//
	// 与 dns_cache（缓存 tcp::resolver 解析结果）不同，这里缓存的是
	// 完整 DNS 响应报文（已剥离事务 ID），供 UDP DNS 服务器与 HTTP
	// DNS 路径共享，命中后改写事务 ID 直接返回，避免重复向上游查询。
	//
	// 行为（与 gproxy 的 dnsCache 保持一致）：
	// - 缓存条数上限（max_items），0 表示不启用缓存；
	// - 过期时间（ttl 秒），命中后重置过期时间（滑动 TTL）；
	// - 超过条数上限按最近最少使用（LRU）淘汰；
	// - 访问时惰性清理已过期条目。
	class dns_response_cache
	{
		dns_response_cache(const dns_response_cache&) = delete;
		dns_response_cache& operator=(const dns_response_cache&) = delete;

	public:
		using key_type = std::string;
		using value_type = std::string;

		struct entry
		{
			value_type resp;                          // 剥离事务 ID 的响应报文.
			std::chrono::steady_clock::time_point expire; // 过期时间点.
		};

		using list_type = std::list<std::pair<key_type, entry>>;
		using map_type = std::unordered_map<key_type, list_type::iterator>;

	public:
		// max_items 为条数上限（0 表示不启用缓存），ttl 为过期秒数（0 表示不启用）.
		dns_response_cache(std::size_t max_items, std::size_t ttl)
			: m_capacity(max_items)
			, m_ttl(std::chrono::seconds(ttl))
		{}

		// 是否启用缓存.
		inline bool enabled() const noexcept
		{
			return m_capacity > 0 && m_ttl > std::chrono::seconds::zero();
		}

		// 当前缓存条目数.
		inline std::size_t size() const noexcept
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			return m_cache_map.size();
		}

		// 命中返回缓存的响应；同时重置过期时间并移到 LRU 前端.
		inline std::optional<value_type> get(const key_type& key)
		{
			std::lock_guard<std::mutex> lock(m_mutex);

			// 惰性清理已过期条目（避免过期条目占用容量）.
			evict_expired();

			auto it = m_cache_map.find(key);
			if (it == m_cache_map.end())
				return std::nullopt;

			// 滑动 TTL：命中后重置过期时间.
			auto& node = *it->second;
			node.second.expire =
				std::chrono::steady_clock::now() + m_ttl;

			// 移到 LRU 前端.
			m_lru_list.splice(m_lru_list.begin(), m_lru_list, it->second);
			it->second = m_lru_list.begin();

			return node.second.resp;
		}

		// 写入（或更新）key 的响应；超限时淘汰最久未使用的条目.
		inline void put(const key_type& key, value_type resp)
		{
			std::lock_guard<std::mutex> lock(m_mutex);

			// 惰性清理已过期条目（避免过期条目占用容量）.
			evict_expired();

			auto now = std::chrono::steady_clock::now();

			auto it = m_cache_map.find(key);
			if (it != m_cache_map.end())
			{
				// 已存在：更新并移到最前面.
				it->second->second.resp = std::move(resp);
				it->second->second.expire = now + m_ttl;
				m_lru_list.splice(m_lru_list.begin(), m_lru_list, it->second);
				it->second = m_lru_list.begin();
				return;
			}

			// 超出容量, 淘汰最久未使用的.
			if (m_capacity > 0 && m_cache_map.size() >= m_capacity)
			{
				auto& node = m_lru_list.back();
				m_cache_map.erase(node.first);
				m_lru_list.pop_back();
			}

			// 新条目插到最前面.
			m_lru_list.emplace_front(key, entry{ std::move(resp), now + m_ttl });
			m_cache_map[m_lru_list.front().first] = m_lru_list.begin();
		}

		// 清空缓存.
		inline void clear() noexcept
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_lru_list.clear();
			m_cache_map.clear();
		}

	private:
		// 惰性清理所有已过期条目（须在持锁状态下调用）.
		void evict_expired() noexcept
		{
			auto now = std::chrono::steady_clock::now();
			auto it = m_cache_map.begin();
			while (it != m_cache_map.end())
			{
				if (now > it->second->second.expire)
				{
					m_lru_list.erase(it->second);
					it = m_cache_map.erase(it);
				}
				else
				{
					++it;
				}
			}
		}

	private:
		mutable std::mutex m_mutex;
		list_type m_lru_list;
		map_type m_cache_map;
		const std::size_t m_capacity;
		const std::chrono::seconds m_ttl;
	};

}

#endif // INCLUDE__2026_08_10__DNS_RESPONSE_CACHE_HPP
