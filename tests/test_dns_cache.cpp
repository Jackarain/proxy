//
// test_dns_cache.cpp
// ~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/dns_cache.hpp"
#include "proxy/dns_response_cache.hpp"

#include <boost/test/unit_test.hpp>

#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>


namespace {

	using namespace proxy;

	// 空 results_type 足以验证缓存的键与容量语义.
	dns_result make_result()
	{
		return dns_result{
			tcp::resolver::results_type{}, std::chrono::steady_clock::now() };
	}

	BOOST_AUTO_TEST_SUITE(dns_cache_suite)

	BOOST_AUTO_TEST_CASE(rejects_zero_capacity)
	{
		BOOST_CHECK_THROW(dns_cache(0), std::invalid_argument);
	}

	BOOST_AUTO_TEST_CASE(put_get_and_miss)
	{
		dns_cache cache(4);
		BOOST_TEST(cache.size() == 0u);
		BOOST_TEST(cache.max_size() == 4u);

		BOOST_TEST(!cache.get("missing").has_value());

		cache.put("example.com", make_result());
		BOOST_TEST(cache.size() == 1u);
		BOOST_TEST(cache.get("example.com").has_value());
	}

	BOOST_AUTO_TEST_CASE(evicts_least_recently_used)
	{
		dns_cache cache(2);

		cache.put("a", make_result());
		cache.put("b", make_result());

		// 访问 a 使其成为最近使用, 再插入 c 应淘汰 b.
		BOOST_TEST(cache.get("a").has_value());
		cache.put("c", make_result());

		BOOST_TEST(cache.size() == 2u);
		BOOST_TEST(cache.get("a").has_value());
		BOOST_TEST(!cache.get("b").has_value());
		BOOST_TEST(cache.get("c").has_value());
	}

	BOOST_AUTO_TEST_SUITE_END()

	BOOST_AUTO_TEST_SUITE(dns_response_cache_suite)

	BOOST_AUTO_TEST_CASE(enabled_requires_capacity_and_ttl)
	{
		BOOST_TEST(!dns_response_cache(0, 60).enabled());
		BOOST_TEST(!dns_response_cache(10, 0).enabled());
		BOOST_TEST(dns_response_cache(10, 60).enabled());
	}

	BOOST_AUTO_TEST_CASE(put_get_and_clear)
	{
		dns_response_cache cache(4, 60);
		BOOST_TEST(cache.size() == 0u);
		BOOST_TEST(!cache.get("k").has_value());

		cache.put("k", "response");
		BOOST_TEST(cache.size() == 1u);

		auto value = cache.get("k");
		BOOST_TEST(value.has_value());
		BOOST_TEST(*value == "response");

		cache.clear();
		BOOST_TEST(cache.size() == 0u);
		BOOST_TEST(!cache.get("k").has_value());
	}

	BOOST_AUTO_TEST_CASE(updates_existing_key_in_place)
	{
		dns_response_cache cache(4, 60);
		cache.put("k", "first");
		cache.put("k", "second");

		BOOST_TEST(cache.size() == 1u);
		BOOST_TEST(cache.get("k").value() == "second");
	}

	BOOST_AUTO_TEST_CASE(evicts_least_recently_used)
	{
		dns_response_cache cache(2, 60);

		cache.put("a", "A");
		cache.put("b", "B");
		BOOST_TEST(cache.get("a").has_value()); // 提升 a 的使用顺序

		cache.put("c", "C");

		BOOST_TEST(cache.size() == 2u);
		BOOST_TEST(cache.get("a").has_value());
		BOOST_TEST(!cache.get("b").has_value());
		BOOST_TEST(cache.get("c").has_value());
	}

	BOOST_AUTO_TEST_CASE(expired_entry_is_dropped)
	{
		dns_response_cache cache(4, 1); // TTL = 1 秒
		cache.put("k", "v");
		BOOST_TEST(cache.get("k").has_value());

		std::this_thread::sleep_for(std::chrono::milliseconds(1100));

		BOOST_TEST(!cache.get("k").has_value());
		BOOST_TEST(cache.size() == 0u);
	}

	BOOST_AUTO_TEST_SUITE_END()

} // namespace
