//
// test_url_info.cpp
// ~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/url_info.hpp"

#include <boost/test/unit_test.hpp>

#include <string>


namespace {

	using namespace proxy;

	BOOST_AUTO_TEST_SUITE(url_info_suite)

	BOOST_AUTO_TEST_CASE(parses_full_url)
	{
		auto result = parse_urlinfo("http://user:pass@example.com:8080/path?q=1#frag");
		BOOST_TEST(result.has_value());

		auto [scheme, user, passwd, host, port, resource] = *result;
		BOOST_TEST(scheme == "http");
		BOOST_TEST(user == "user");
		BOOST_TEST(passwd == "pass");
		BOOST_TEST(host == "example.com");
		BOOST_TEST(port == 8080);
		// fragment 不属于 resource.
		BOOST_TEST(resource == "/path?q=1");
	}

	BOOST_AUTO_TEST_CASE(uses_scheme_default_port)
	{
		auto http = parse_urlinfo("http://example.com/index.html");
		BOOST_TEST(http.has_value());
		BOOST_TEST(std::get<4>(*http) == 80);
		BOOST_TEST(std::get<5>(*http) == "/index.html");

		auto https = parse_urlinfo("https://example.com");
		BOOST_TEST(https.has_value());
		BOOST_TEST(std::get<4>(*https) == 443);

		auto unknown = parse_urlinfo("socks5://example.com");
		BOOST_TEST(unknown.has_value());
		BOOST_TEST(std::get<4>(*unknown) == 0);
	}

	BOOST_AUTO_TEST_CASE(parses_url_without_userinfo)
	{
		auto result = parse_urlinfo("https://example.com:8443");
		BOOST_TEST(result.has_value());

		auto [scheme, user, passwd, host, port, resource] = *result;
		BOOST_TEST(scheme == "https");
		BOOST_TEST(user.empty());
		BOOST_TEST(passwd.empty());
		BOOST_TEST(host == "example.com");
		BOOST_TEST(port == 8443);
		BOOST_TEST(resource.empty());
	}

	BOOST_AUTO_TEST_CASE(parses_userinfo_without_password)
	{
		auto result = parse_urlinfo("http://user@example.com/");
		BOOST_TEST(result.has_value());

		auto [scheme, user, passwd, host, port, resource] = *result;
		BOOST_TEST(user == "user");
		BOOST_TEST(passwd.empty());
		BOOST_TEST(host == "example.com");
		BOOST_TEST(resource == "/");
	}

	BOOST_AUTO_TEST_CASE(parses_ipv6_authority)
	{
		auto result = parse_urlinfo(
			"http://user:pass@[2001:db8::1]:8080/path?q=1");
		BOOST_TEST(result.has_value());

		auto [scheme, user, passwd, host, port, resource] = *result;
		BOOST_TEST(scheme == "http");
		BOOST_TEST(user == "user");
		BOOST_TEST(passwd == "pass");
		BOOST_TEST(host == "2001:db8::1");
		BOOST_TEST(port == 8080);
		BOOST_TEST(resource == "/path?q=1");

		auto default_port = parse_urlinfo("https://[::1]/");
		BOOST_TEST(default_port.has_value());
		BOOST_TEST(std::get<3>(*default_port) == "::1");
		BOOST_TEST(std::get<4>(*default_port) == 443);
	}

	BOOST_AUTO_TEST_CASE(rejects_invalid_input)
	{
		BOOST_TEST(!parse_urlinfo("example.com").has_value());
		BOOST_TEST(!parse_urlinfo("http://").has_value());
		BOOST_TEST(!parse_urlinfo("http://example.com:abc/").has_value());
		BOOST_TEST(!parse_urlinfo("http://example.com:0/").has_value());
		BOOST_TEST(!parse_urlinfo("http://example.com:65536/").has_value());
		BOOST_TEST(!parse_urlinfo("http://2001:db8::1/").has_value());
		BOOST_TEST(!parse_urlinfo("http://[2001:db8::1/").has_value());
		BOOST_TEST(!parse_urlinfo("http://[]:80/").has_value());
		BOOST_TEST(!parse_urlinfo("http://[::1]x/").has_value());
		BOOST_TEST(!parse_urlinfo("http://[::1]:abc/").has_value());
		BOOST_TEST(!parse_urlinfo("").has_value());
	}

	BOOST_AUTO_TEST_SUITE_END()

} // namespace
