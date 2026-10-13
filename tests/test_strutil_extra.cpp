//
// test_strutil_extra.cpp
// ~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/strutil.hpp"

#include <boost/test/unit_test.hpp>

#include <regex>
#include <string>
#include <vector>


namespace {

	BOOST_AUTO_TEST_SUITE(strutil_extra_suite)

	BOOST_AUTO_TEST_CASE(base64_encode_all_paddings)
	{
		BOOST_TEST(strutil::base64_encode(std::string()) == "");
		BOOST_TEST(strutil::base64_encode(std::string("f")) == "Zg==");
		BOOST_TEST(strutil::base64_encode(std::string("fo")) == "Zm8=");
		BOOST_TEST(strutil::base64_encode(std::string("foo")) == "Zm9v");
		BOOST_TEST(strutil::base64_encode(std::string("hello")) == "aGVsbG8=");
	}

	BOOST_AUTO_TEST_CASE(base64_round_trip_binary)
	{
		const std::string binary{ '\x00', '\x01', '\x7f', '\x80', '\xff' };
		auto encoded = strutil::base64_encode(binary);
		BOOST_TEST(strutil::base64_decode(encoded) == binary);
	}

	BOOST_AUTO_TEST_CASE(base64_character_class)
	{
		BOOST_TEST(strutil::is_base64('A'));
		BOOST_TEST(strutil::is_base64('z'));
		BOOST_TEST(strutil::is_base64('0'));
		BOOST_TEST(strutil::is_base64('+'));
		BOOST_TEST(strutil::is_base64('/'));
		BOOST_TEST(!strutil::is_base64('='));
		BOOST_TEST(!strutil::is_base64(' '));
		BOOST_TEST(!strutil::is_base64('-'));
	}

	BOOST_AUTO_TEST_CASE(is_ipv4_host_accepts_dotted_quads)
	{
		BOOST_TEST(strutil::is_ipv4_host("127.0.0.1"));
		BOOST_TEST(strutil::is_ipv4_host("192.168.1.1"));
		BOOST_TEST(strutil::is_ipv4_host("0.0.0.0"));
		BOOST_TEST(strutil::is_ipv4_host("255.255.255.255"));

		BOOST_TEST(!strutil::is_ipv4_host(""));
		BOOST_TEST(!strutil::is_ipv4_host("1.2.3.4.5"));
	}

	BOOST_AUTO_TEST_CASE(is_ipv6_host_accepts_literals)
	{
		BOOST_TEST(strutil::is_ipv6_host("::1"));
		BOOST_TEST(strutil::is_ipv6_host("2001:db8::1"));
		BOOST_TEST(strutil::is_ipv6_host("fe80::1"));
		BOOST_TEST(strutil::is_ipv6_host("::ffff:192.168.1.1"));

		BOOST_TEST(!strutil::is_ipv6_host(""));
		BOOST_TEST(!strutil::is_ipv6_host("1.2.3.4"));
		BOOST_TEST(!strutil::is_ipv6_host("abcd"));
	}

	BOOST_AUTO_TEST_CASE(regex_helpers)
	{
		BOOST_TEST(strutil::matches("abc123", std::regex("[a-z]+[0-9]+")));
		BOOST_TEST(!strutil::matches("123abc", std::regex("[a-z]+[0-9]+")));
		BOOST_TEST(!strutil::matches("abc", std::regex("[0-9]+")));

		auto tokens = strutil::regex_split("a1b22c", "[0-9]+");
		BOOST_TEST(tokens.size() == 3u);
		BOOST_TEST(tokens[0] == "a");
		BOOST_TEST(tokens[1] == "b");
		BOOST_TEST(tokens[2] == "c");
	}

	BOOST_AUTO_TEST_CASE(valid_utf_accepts_and_rejects)
	{
		unsigned char ascii[] = { 'a', 'b', 'c' };
		BOOST_TEST(strutil::valid_utf(ascii, 3));

		unsigned char euro[] = { 0xE2, 0x82, 0xAC };
		BOOST_TEST(strutil::valid_utf(euro, 3));

		unsigned char emoji[] = { 0xF0, 0x9F, 0x98, 0x80 };
		BOOST_TEST(strutil::valid_utf(emoji, 4));

		unsigned char lone_continuation[] = { 0x80 };
		BOOST_TEST(!strutil::valid_utf(lone_continuation, 1));

		unsigned char overlong[] = { 0xC0, 0x80 };
		BOOST_TEST(!strutil::valid_utf(overlong, 2));

		unsigned char invalid[] = { 0xFE };
		BOOST_TEST(!strutil::valid_utf(invalid, 1));

		unsigned char truncated[] = { 0xC3 };
		BOOST_TEST(!strutil::valid_utf(truncated, 1));
	}

	BOOST_AUTO_TEST_CASE(utf8_utf16_round_trip)
	{
		auto wide = strutil::utf8_utf16("abc");
		BOOST_TEST(wide.has_value());
		bool wide_ok = wide.has_value() && *wide == L"abc";
		BOOST_TEST(wide_ok);

		auto narrow = strutil::utf16_utf8(std::wstring_view(L"abc"));
		BOOST_TEST(narrow.has_value());
		BOOST_TEST(*narrow == "abc");

		// "中文" -> U+4E2D U+6587
		const std::string cjk = "\xe4\xb8\xad\xe6\x96\x87";
		auto wide_cjk = strutil::utf8_utf16(cjk);
		BOOST_TEST(wide_cjk.has_value());
		BOOST_TEST(wide_cjk->size() == 2u);
		BOOST_TEST(static_cast<int>((*wide_cjk)[0]) == 0x4E2D);
		BOOST_TEST(static_cast<int>((*wide_cjk)[1]) == 0x6587);

		auto back = strutil::utf16_utf8(*wide_cjk);
		BOOST_TEST(back.has_value());
		BOOST_TEST(*back == cjk);
	}

	BOOST_AUTO_TEST_CASE(utf8_utf16_surrogate_pair)
	{
		const std::string emoji = "\xf0\x9f\x98\x80"; // U+1F600

		BOOST_TEST(strutil::count_utf8_to_utf16(emoji) == 2u);

		auto wide = strutil::utf8_utf16(emoji);
		BOOST_TEST(wide.has_value());
		BOOST_TEST(wide->size() == 2u);

		auto back = strutil::utf16_utf8(*wide);
		BOOST_TEST(back.has_value());
		BOOST_TEST(*back == emoji);
	}

	BOOST_AUTO_TEST_CASE(utf_count_helpers)
	{
		BOOST_TEST(strutil::count_utf8_to_utf16("abc") == 3u);
		BOOST_TEST(strutil::count_utf16_to_utf8(std::wstring_view(L"abc")) == 3u);
	}

	BOOST_AUTO_TEST_CASE(vector_helpers_in_place)
	{
		std::vector<std::string> values{ "a", "", "b", "" };
		strutil::drop_empty(values);
		BOOST_TEST(values.size() == 2u);
		BOOST_TEST(values[0] == "a");
		BOOST_TEST(values[1] == "b");

		std::vector<std::string> duplicates{ "b", "a", "b", "a", "c" };
		strutil::drop_duplicate(duplicates);
		BOOST_TEST(duplicates.size() == 3u);
		BOOST_TEST(duplicates[0] == "a");
		BOOST_TEST(duplicates[2] == "c");

		std::vector<int> numbers{ 1, 2, 3 };
		auto reversed = strutil::reverse_copy(numbers);
		BOOST_TEST(reversed[0] == 3);
		BOOST_TEST(reversed[2] == 1);
		BOOST_TEST(numbers[0] == 1);
	}

	BOOST_AUTO_TEST_CASE(parse_and_format_numbers)
	{
		BOOST_TEST(strutil::parse_string<int>("42") == 42);
		BOOST_TEST(strutil::parse_string<double>("2.5") == 2.5);
		BOOST_TEST(strutil::parse_string<int>("not-a-number") == 0);

		BOOST_TEST(strutil::from_string("10", 10).value() == 10);
		BOOST_TEST(strutil::from_string("ff", 16).value() == 255);
		BOOST_TEST(strutil::from_string("777", 8).value() == 511);
	}

	BOOST_AUTO_TEST_SUITE_END()

} // namespace
