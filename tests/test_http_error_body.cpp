//
// test_http_error_body.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/http_error_body.hpp"

#include <boost/test/unit_test.hpp>

#include <cstdlib>
#include <string>
#include <string_view>


namespace {

	using namespace proxy;

	// 返回第一个空行之后的 HTTP 正文部分.
	std::string_view http_body(std::string_view response)
	{
		auto pos = response.find("\n\n");
		if (pos == std::string_view::npos)
			return {};
		return response.substr(pos + 2);
	}

	// 解析响应头中声明的 Content-Length, 未找到返回 -1.
	long declared_content_length(std::string_view response)
	{
		constexpr std::string_view key = "Content-Length: ";
		auto pos = response.find(key);
		if (pos == std::string_view::npos)
			return -1;

		pos += key.size();
		auto end = response.find_first_of("\r\n", pos);
		if (end == std::string_view::npos)
			end = response.size();

		return std::strtol(
			std::string(response.substr(pos, end - pos)).c_str(), nullptr, 10);
	}

	BOOST_AUTO_TEST_SUITE(http_error_body_suite)

	BOOST_AUTO_TEST_CASE(full_responses_declare_correct_body_length)
	{
		BOOST_TEST(http_body(fake_400_content_fmt).size() ==
			static_cast<size_t>(declared_content_length(fake_400_content_fmt)));
		BOOST_TEST(http_body(fake_404_content_fmt).size() ==
			static_cast<size_t>(declared_content_length(fake_404_content_fmt)));
		BOOST_TEST(http_body(fake_407_content_fmt).size() ==
			static_cast<size_t>(declared_content_length(fake_407_content_fmt)));
	}

	BOOST_AUTO_TEST_CASE(full_responses_start_with_status_line)
	{
		BOOST_TEST(std::string_view(fake_400_content_fmt).starts_with(
			"HTTP/1.1 400 Bad Request"));
		BOOST_TEST(std::string_view(fake_404_content_fmt).starts_with(
			"HTTP/1.1 404 Not Found"));
		BOOST_TEST(std::string_view(fake_407_content_fmt).starts_with(
			"HTTP/1.1 407 Proxy Authentication Required"));
	}

	BOOST_AUTO_TEST_CASE(embedded_400_body_matches_standalone_body)
	{
		// fake_400_content_fmt 的正文应与独立的 fake_400_content 完全一致.
		BOOST_TEST(std::string(http_body(fake_400_content_fmt)) ==
			std::string(fake_400_content));
	}

	BOOST_AUTO_TEST_CASE(bodies_carry_expected_markers)
	{
		auto contains = [](const char* content, std::string_view needle)
		{
			return std::string_view(content).find(needle) != std::string_view::npos;
		};

		BOOST_TEST(contains(fake_500_content_fmt,
			"<h1>500 Internal Server Error</h1>"));
		BOOST_TEST(contains(fake_400_content, "<h1>400 Bad Request</h1>"));
		BOOST_TEST(contains(fake_401_content,
			"<h1>401 Authorization Required</h1>"));
		BOOST_TEST(contains(fake_403_content, "<h1>403 Forbidden</h1>"));
		BOOST_TEST(contains(fake_405_content, "<h1>405 Method Not Allowed</h1>"));
		BOOST_TEST(contains(fake_416_content,
			"<h1>416 Requested Range Not Satisfiable</h1>"));
		BOOST_TEST(contains(fake_502_dns_content, "DNS resolution failed."));
		BOOST_TEST(contains(fake_502_socks_connect_content,
			"Cannot connect to upstream SOCKS proxy."));
		BOOST_TEST(contains(fake_502_ssl_config_content,
			"SSL configuration failed."));
		BOOST_TEST(contains(fake_502_ssl_handshake_content,
			"SSL handshake with upstream SOCKS proxy failed."));
		BOOST_TEST(contains(fake_502_socks_associate_content,
			"SOCKS5 UDP ASSOCIATE failed."));
		BOOST_TEST(contains(fake_502_proxy_connect_content,
			"Cannot connect to upstream proxy."));
		BOOST_TEST(contains(fake_403_forbidden_content, "UDP protocol disabled."));
	}

	BOOST_AUTO_TEST_SUITE_END()

} // namespace
