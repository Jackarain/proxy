//
// test_ssl_client.cpp
// ~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/ssl_client.hpp"

#include <boost/test/unit_test.hpp>

#include <string>


namespace {

	using namespace proxy;

	BOOST_AUTO_TEST_SUITE(ssl_client_suite)

	BOOST_AUTO_TEST_CASE(cache_shares_context_across_setup_types)
	{
		int setup_calls = 0;

		auto setup = [&setup_calls](net::ssl::context&) noexcept
		{
			setup_calls++;
			return boost::system::error_code{};
		};

		auto first = get_cached_ssl_client_ctx("unit-test-shared-tag", setup);
		BOOST_TEST(!first.has_error());
		BOOST_TEST(setup_calls == 1);

		// 不同的 lambda 类型(不同 Setup 模板参数)必须命中同一份缓存,
		// 这正是把缓存移出模板函数所保证的契约.
		auto second = get_cached_ssl_client_ctx("unit-test-shared-tag",
			[](net::ssl::context&) noexcept
			{
				return boost::system::error_code{};
			});

		BOOST_TEST(!second.has_error());
		BOOST_TEST(first.value().get() == second.value().get());
		BOOST_TEST(setup_calls == 1);
	}

	BOOST_AUTO_TEST_CASE(different_tags_get_different_contexts)
	{
		auto a = get_cached_ssl_client_ctx("unit-test-tag-a",
			[](net::ssl::context&) noexcept
			{
				return boost::system::error_code{};
			});
		auto b = get_cached_ssl_client_ctx("unit-test-tag-b",
			[](net::ssl::context&) noexcept
			{
				return boost::system::error_code{};
			});

		BOOST_TEST(!a.has_error());
		BOOST_TEST(!b.has_error());
		BOOST_TEST(a.value().get() != b.value().get());
	}

	BOOST_AUTO_TEST_CASE(failed_setup_is_not_cached)
	{
		int attempts = 0;
		auto setup = [&attempts](net::ssl::context&) noexcept
		{
			attempts++;
			return boost::system::errc::make_error_code(
				boost::system::errc::invalid_argument);
		};

		auto first = get_cached_ssl_client_ctx("unit-test-error-tag", setup);
		auto second = get_cached_ssl_client_ctx("unit-test-error-tag", setup);

		BOOST_TEST(first.has_error());
		BOOST_TEST(second.has_error());
		BOOST_TEST(attempts == 2);
	}

	BOOST_AUTO_TEST_CASE(shared_client_ctx_is_stable_per_hostname)
	{
		auto a1 = get_shared_ssl_client_ctx(true, "unit-test.example.com");
		auto a2 = get_shared_ssl_client_ctx(true, "unit-test.example.com");
		auto b = get_shared_ssl_client_ctx(true, "unit-test-other.example.com");

		BOOST_TEST(!a1.has_error());
		BOOST_TEST(!a2.has_error());
		BOOST_TEST(!b.has_error());

		BOOST_TEST(a1.value().get() == a2.value().get());
		BOOST_TEST(a1.value().get() != b.value().get());
	}

	BOOST_AUTO_TEST_SUITE_END()

} // namespace
