//
// test_strutil.cpp
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/strutil.hpp"

#include <boost/test/unit_test.hpp>

#include <string>
#include <vector>


namespace {

	BOOST_AUTO_TEST_SUITE(strutil_suite)

	BOOST_AUTO_TEST_CASE(case_conversions)
	{
		BOOST_TEST(strutil::to_lower("AbC") == "abc");
		BOOST_TEST(strutil::to_upper("AbC") == "ABC");
		BOOST_TEST(strutil::capitalize("hello world") == "Hello world");
		BOOST_TEST(strutil::capitalize_first_char("hELLO WORLD") == "Hello world");
		BOOST_TEST(strutil::capitalize("") == std::string());
	}

	BOOST_AUTO_TEST_CASE(contains_and_compare)
	{
		BOOST_TEST(strutil::contains("hello", "ell"));
		BOOST_TEST(strutil::contains("hello", 'l'));
		BOOST_TEST(!strutil::contains("hello", 'z'));

		BOOST_TEST(strutil::compare_ignore_case("Hello", "hELLO"));
		BOOST_TEST(!strutil::compare_ignore_case("Hello", "world"));

		BOOST_TEST(strutil::starts_with("hello", "he"));
		BOOST_TEST(strutil::ends_with("hello", "lo"));
		BOOST_TEST(strutil::ends_with("hello", 'o'));
		BOOST_TEST(!strutil::ends_with("hello", "he"));
	}

	BOOST_AUTO_TEST_CASE(trim_and_remove_spaces)
	{
		BOOST_TEST(strutil::trim_copy("  hi  ") == "hi");
		BOOST_TEST(strutil::trim_left_copy("  hi  ") == "hi  ");
		BOOST_TEST(strutil::trim_right_copy("  hi  ") == "  hi");
		BOOST_TEST(strutil::remove_spaces(" a b\tc ") == "abc");

		std::string value = "\t value \n";
		strutil::trim(value);
		BOOST_TEST(value == "value");

		BOOST_TEST(strutil::string_trim("  hi  ") == "hi");
		BOOST_TEST(strutil::string_trim_left("  hi  ") == "hi  ");
		BOOST_TEST(strutil::string_trim_right("  hi  ") == "  hi");
	}

	BOOST_AUTO_TEST_CASE(replace_helpers)
	{
		std::string text = "a-b-c";

		BOOST_TEST(strutil::replace_first(text, "-", "+"));
		BOOST_TEST(text == "a+b-c");

		text = "a-b-c";
		BOOST_TEST(strutil::replace_last(text, "-", "+"));
		BOOST_TEST(text == "a-b+c");

		text = "a-b-c";
		BOOST_TEST(strutil::replace_all(text, "-", "+"));
		BOOST_TEST(text == "a+b+c");

		text = "abc";
		BOOST_TEST(!strutil::replace_first(text, "z", "x"));
	}

	BOOST_AUTO_TEST_CASE(split_and_join)
	{
		auto tokens = strutil::split("a,b,c", ',');
		BOOST_TEST(tokens.size() == 3u);
		BOOST_TEST(tokens[0] == "a");
		BOOST_TEST(tokens[2] == "c");

		// 连续分隔符产生空 token.
		auto empty_tokens = strutil::split("a,,c", ',');
		BOOST_TEST(empty_tokens.size() == 3u);
		BOOST_TEST(empty_tokens[1].empty());

		// 空输入返回单个空 token.
		auto none = strutil::split(std::string(), ',');
		BOOST_TEST(none.size() == 1u);
		BOOST_TEST(none[0].empty());

		auto multi = strutil::split("a::b::c", std::string("::"));
		BOOST_TEST(multi.size() == 3u);
		BOOST_TEST(multi[1] == "b");

		auto any = strutil::split_any("a,b;c", ",;");
		BOOST_TEST(any.size() == 3u);
		BOOST_TEST(any[2] == "c");

		BOOST_TEST(strutil::join(tokens, "-") == "a-b-c");

		auto kept = strutil::drop_empty_copy(std::vector<std::string>{ "a", "", "b" });
		BOOST_TEST(kept.size() == 2u);

		auto unique = strutil::drop_duplicate_copy(
			std::vector<std::string>{ "b", "a", "b", "c" });
		BOOST_TEST(unique.size() == 3u);
		BOOST_TEST(unique[0] == "a");
		BOOST_TEST(unique[2] == "c");
	}

	BOOST_AUTO_TEST_CASE(repeat_and_sort)
	{
		BOOST_TEST(strutil::repeat("ab", 3) == "ababab");
		BOOST_TEST(strutil::repeat('x', 4) == "xxxx");

		std::vector<int> values{ 3, 1, 2 };
		strutil::sorting_ascending(values);
		BOOST_TEST(values[0] == 1);
		BOOST_TEST(values[2] == 3);

		strutil::sorting_descending(values);
		BOOST_TEST(values[0] == 3);
		BOOST_TEST(values[2] == 1);

		strutil::reverse_inplace(values);
		BOOST_TEST(values[0] == 1);
	}

	BOOST_AUTO_TEST_CASE(ascii_character_classification)
	{
		BOOST_TEST(strutil::isalpha('a') == 1);
		BOOST_TEST(strutil::isalpha('Z') == 1);
		BOOST_TEST(strutil::isalpha('1') == 0);
		BOOST_TEST(strutil::isdigit('7') == 1);
		BOOST_TEST(strutil::isdigit('a') == 0);
		BOOST_TEST(strutil::isalnum('a') == 1);
		BOOST_TEST(strutil::isalnum('7') == 1);
		BOOST_TEST(strutil::isalnum('-') == 0);
		BOOST_TEST(strutil::is_space(' '));
		BOOST_TEST(strutil::is_space('\n'));
		BOOST_TEST(!strutil::is_space('a'));
	}

	BOOST_AUTO_TEST_CASE(hex_helpers)
	{
		const std::string data{ '\x01', '\xab' };
		BOOST_TEST(strutil::to_hex(std::string_view(data)) == "01ab");
		BOOST_TEST(strutil::to_hex_prefixed(std::string_view(data)) == "0x01ab");

		BOOST_TEST(strutil::from_hex_char('f') == 15);
		BOOST_TEST(strutil::from_hex_char('F') == 15);
		BOOST_TEST(strutil::from_hex_char('0') == 0);
		BOOST_TEST(strutil::from_hex_char('z') == static_cast<char>(-1));

		std::vector<uint8_t> bytes;
		BOOST_TEST(strutil::from_hexstring("01ab", bytes));
		BOOST_TEST(bytes.size() == 2u);
		BOOST_TEST(bytes[0] == 0x01);
		BOOST_TEST(bytes[1] == 0xab);

		bytes.clear();
		BOOST_TEST(strutil::from_hexstring("0x01ab", bytes));
		BOOST_TEST(bytes.size() == 2u);

		bytes.clear();
		BOOST_TEST(!strutil::from_hexstring("zz", bytes));

		BOOST_TEST(strutil::is_hexstring("0x1f"));
		BOOST_TEST(strutil::is_hexstring("1f"));
		BOOST_TEST(!strutil::is_hexstring("0xzz"));
	}

	BOOST_AUTO_TEST_CASE(unescape_and_format_helpers)
	{
		std::string out;
		BOOST_TEST(strutil::unescape("%41%42", out));
		BOOST_TEST(out == "AB");

		BOOST_TEST(strutil::unescape("a+b", out));
		BOOST_TEST(out == "a b");

		BOOST_TEST(!strutil::unescape("%zz", out));

		BOOST_TEST(strutil::to_string(1.5f, 8) == "   1.500");
		BOOST_TEST(strutil::add_suffix(2048.f) == "2.000kB");
	}

	BOOST_AUTO_TEST_SUITE_END()

} // namespace
