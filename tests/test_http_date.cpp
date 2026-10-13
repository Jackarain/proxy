//
// test_http_date.cpp
// ~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/http_date.hpp"

#include <boost/test/unit_test.hpp>

#include <ctime>
#include <string>
#include <string_view>


namespace {

	using namespace proxy;

	// 1994-11-06 08:49:37 UTC, HTTP-date 规范中的示例时间.
	constexpr std::time_t kExampleTime = 784111777;

	BOOST_AUTO_TEST_SUITE(http_date_suite)

	BOOST_AUTO_TEST_CASE(http_date_string_formats_imf_fixdate)
	{
		BOOST_TEST(http_date_string(kExampleTime) ==
			"Sun, 06 Nov 1994 08:49:37 GMT");
		BOOST_TEST(http_date_string(0) ==
			"Thu, 01 Jan 1970 00:00:00 GMT");
	}

	BOOST_AUTO_TEST_CASE(http_month_from_name_matches_case_insensitively)
	{
		BOOST_TEST(http_month_from_name("Jan") == 1);
		BOOST_TEST(http_month_from_name("Nov") == 11);
		BOOST_TEST(http_month_from_name("dec") == 12);
		BOOST_TEST(http_month_from_name("DEC") == 12);
		BOOST_TEST(http_month_from_name("XXX") == 0);
	}

	BOOST_AUTO_TEST_CASE(make_utc_time_validates_fields)
	{
		auto value = make_utc_time(1994, 11, 6, 8, 49, 37);
		BOOST_TEST(value.has_value());
		BOOST_TEST(*value == kExampleTime);

		BOOST_TEST(!make_utc_time(1969, 1, 1, 0, 0, 0).has_value());
		BOOST_TEST(!make_utc_time(2000, 0, 1, 0, 0, 0).has_value());
		BOOST_TEST(!make_utc_time(2000, 13, 1, 0, 0, 0).has_value());
		BOOST_TEST(!make_utc_time(2000, 1, 0, 0, 0, 0).has_value());
		BOOST_TEST(!make_utc_time(2000, 1, 32, 0, 0, 0).has_value());
		BOOST_TEST(!make_utc_time(2000, 1, 1, 24, 0, 0).has_value());
		BOOST_TEST(!make_utc_time(2000, 1, 1, 0, 60, 0).has_value());
		BOOST_TEST(!make_utc_time(2000, 1, 1, 0, 0, 61).has_value());
	}

	BOOST_AUTO_TEST_CASE(parse_http_date_accepts_imf_fixdate)
	{
		auto value = parse_http_date("Sun, 06 Nov 1994 08:49:37 GMT");
		BOOST_TEST(value.has_value());
		BOOST_TEST(*value == kExampleTime);

		// 首尾空白被忽略.
		auto padded = parse_http_date("  Sun, 06 Nov 1994 08:49:37 GMT \t");
		BOOST_TEST(padded.has_value());
		BOOST_TEST(*padded == kExampleTime);
	}

	BOOST_AUTO_TEST_CASE(parse_http_date_accepts_rfc850)
	{
		auto value = parse_http_date("Sunday, 06-Nov-94 08:49:37 GMT");
		BOOST_TEST(value.has_value());
		BOOST_TEST(*value == kExampleTime);
	}

	BOOST_AUTO_TEST_CASE(parse_http_date_accepts_asctime)
	{
		auto value = parse_http_date("Sun Nov  6 08:49:37 1994");
		BOOST_TEST(value.has_value());
		BOOST_TEST(*value == kExampleTime);
	}

	BOOST_AUTO_TEST_CASE(parse_http_date_rejects_invalid_values)
	{
		BOOST_TEST(!parse_http_date("").has_value());
		BOOST_TEST(!parse_http_date("   ").has_value());
		BOOST_TEST(!parse_http_date("hello world").has_value());
		BOOST_TEST(!parse_http_date("Sun, 06 Xxx 1994 08:49:37 GMT").has_value());
	}

	BOOST_AUTO_TEST_CASE(make_file_etag_formats_hex)
	{
		BOOST_TEST(make_file_etag(0x1f, 0x2a) == "\"1f-2a\"");
		BOOST_TEST(make_file_etag(0, 0) == "\"0-0\"");
	}

	BOOST_AUTO_TEST_CASE(http_etag_match_handles_lists_and_wildcard)
	{
		BOOST_TEST(http_etag_match("*", "\"abc\""));
		BOOST_TEST(http_etag_match("\"abc\"", "\"abc\""));
		BOOST_TEST(http_etag_match("\"abc\", \"def\"", "\"def\""));
		BOOST_TEST(http_etag_match("  \"abc\"  ", "\"abc\""));

		// 弱比较: 忽略 W/ 前缀.
		BOOST_TEST(http_etag_match("W/\"abc\"", "\"abc\""));
		BOOST_TEST(http_etag_match("\"abc\"", "\"abc\""));

		BOOST_TEST(!http_etag_match("", "\"abc\""));
		BOOST_TEST(!http_etag_match("\"abc\"", "\"def\""));
		BOOST_TEST(!http_etag_match("\"abc", "\"abc\""));
	}

	BOOST_AUTO_TEST_SUITE_END()

} // namespace
