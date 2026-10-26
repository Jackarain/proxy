//
// test_proxy_util.cpp
// ~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/proxy_util.hpp"

#include <boost/test/unit_test.hpp>

#include <limits>
#include <string>

#if !defined(BOOST_WINDOWS)
# include <netinet/in.h>
# include <sys/socket.h>
#endif


namespace {

	using namespace proxy;

	BOOST_AUTO_TEST_SUITE(proxy_util_suite)

	BOOST_AUTO_TEST_CASE(parse_cidr_rule_single_address)
	{
		auto rule = parse_cidr_rule("10.0.0.1");
		BOOST_TEST(rule.text == "10.0.0.1");
		BOOST_TEST(rule.single.has_value());
		BOOST_TEST(!rule.v4.has_value());
		BOOST_TEST(!rule.v6.has_value());
		BOOST_TEST(rule.single->to_string() == "10.0.0.1");
	}

	BOOST_AUTO_TEST_CASE(parse_cidr_rule_ipv4_network)
	{
		auto rule = parse_cidr_rule("192.168.1.0/24");
		BOOST_TEST(!rule.single.has_value());
		BOOST_TEST(rule.v4.has_value());
		BOOST_TEST(!rule.v6.has_value());
		BOOST_TEST(rule.v4->network().to_string() == "192.168.1.0");
		BOOST_TEST(rule.v4->prefix_length() == 24);
	}

	BOOST_AUTO_TEST_CASE(parse_cidr_rule_ipv6_network)
	{
		auto rule = parse_cidr_rule("2001:db8::/32");
		BOOST_TEST(!rule.single.has_value());
		BOOST_TEST(!rule.v4.has_value());
		BOOST_TEST(rule.v6.has_value());
		BOOST_TEST(rule.v6->prefix_length() == 32);
	}

	BOOST_AUTO_TEST_CASE(parse_cidr_rule_invalid_never_matches)
	{
		auto rule = parse_cidr_rule("example.com");
		BOOST_TEST(rule.text == "example.com");
		BOOST_TEST(!rule.single.has_value());
		BOOST_TEST(!rule.v4.has_value());
		BOOST_TEST(!rule.v6.has_value());

		auto empty = parse_cidr_rule("");
		BOOST_TEST(!empty.single.has_value());
		BOOST_TEST(!empty.v4.has_value());
		BOOST_TEST(!empty.v6.has_value());
	}

	BOOST_AUTO_TEST_CASE(is_hostname_distinguishes_ip_and_name)
	{
		BOOST_TEST(is_hostname("example.com"));
		BOOST_TEST(is_hostname("localhost"));
		// 不能解析为地址的字符串按域名处理.
		BOOST_TEST(is_hostname("1.2.3"));
		BOOST_TEST(!is_hostname("127.0.0.1"));
		BOOST_TEST(!is_hostname("::1"));
		BOOST_TEST(!is_hostname("2001:db8::1"));
	}

	BOOST_AUTO_TEST_CASE(parse_num_accepts_only_non_negative_decimals)
	{
		BOOST_TEST(parse_num("0").value() == 0);
		BOOST_TEST(parse_num("12345").value() == 12345);
		BOOST_TEST(parse_num("9223372036854775807").value() ==
			std::numeric_limits<int64_t>::max());

		BOOST_TEST(!parse_num("").has_value());
		BOOST_TEST(!parse_num("12a").has_value());
		BOOST_TEST(!parse_num("-1").has_value());
		BOOST_TEST(!parse_num("+1").has_value());
		BOOST_TEST(!parse_num(" 1").has_value());
		// 溢出.
		BOOST_TEST(!parse_num("9223372036854775808").has_value());
		BOOST_TEST(!parse_num("99999999999999999999").has_value());
	}

	BOOST_AUTO_TEST_CASE(is_ascii_space_matches_http_whitespace)
	{
		BOOST_TEST(is_ascii_space(' '));
		BOOST_TEST(is_ascii_space('\t'));
		BOOST_TEST(is_ascii_space('\n'));
		BOOST_TEST(is_ascii_space('\v'));
		BOOST_TEST(is_ascii_space('\f'));
		BOOST_TEST(is_ascii_space('\r'));
		BOOST_TEST(!is_ascii_space('a'));
		BOOST_TEST(!is_ascii_space('\0'));
	}

	BOOST_AUTO_TEST_CASE(proxy_pass_default_port_by_scheme)
	{
		BOOST_TEST(proxy_pass_default_port(urls::url("http://example.com")) == 80);
		BOOST_TEST(proxy_pass_default_port(urls::url("https://example.com")) == 443);
		BOOST_TEST(proxy_pass_default_port(urls::url("ws://example.com")) == 80);
		BOOST_TEST(proxy_pass_default_port(urls::url("wss://example.com")) == 443);

		// 未定义默认端口的 scheme: 非 s 结尾默认 1080.
		BOOST_TEST(proxy_pass_default_port(urls::url("socks5://example.com")) == 1080);
		BOOST_TEST(proxy_pass_default_port(urls::url("myproxy://example.com")) == 1080);
		// s 结尾视为 TLS 默认 443.
		BOOST_TEST(proxy_pass_default_port(urls::url("myproxys://example.com")) == 443);
	}

	BOOST_AUTO_TEST_CASE(custom_write_all_controls_transfer)
	{
		boost::system::error_code ec;
		BOOST_TEST(custom_write_all(ec, 0) ==
			(std::numeric_limits<std::size_t>::max)());

		ec = net::error::operation_aborted;
		BOOST_TEST(custom_write_all(ec, 100) == 0u);
	}

#if !defined(BOOST_WINDOWS)
	BOOST_AUTO_TEST_CASE(sockaddr_to_udp_endpoint_ipv4)
	{
		sockaddr_storage storage{};
		auto* addr4 = reinterpret_cast<sockaddr_in*>(&storage);
		addr4->sin_family = AF_INET;
		addr4->sin_addr.s_addr = htonl(0x7f000001);
		addr4->sin_port = htons(8080);

		auto endpoint = sockaddr_to_udp_endpoint(storage);
		BOOST_TEST(endpoint.address().to_string() == "127.0.0.1");
		BOOST_TEST(endpoint.port() == 8080);
	}

	BOOST_AUTO_TEST_CASE(sockaddr_to_udp_endpoint_ipv6)
	{
		sockaddr_storage storage{};
		auto* addr6 = reinterpret_cast<sockaddr_in6*>(&storage);
		addr6->sin6_family = AF_INET6;
		addr6->sin6_addr.s6_addr[15] = 1; // ::1
		addr6->sin6_port = htons(53);

		auto endpoint = sockaddr_to_udp_endpoint(storage);
		BOOST_TEST(endpoint.address().to_string() == "::1");
		BOOST_TEST(endpoint.port() == 53);
	}

	BOOST_AUTO_TEST_CASE(sockaddr_to_udp_endpoint_unknown_family)
	{
		sockaddr_storage storage{};
		storage.ss_family = AF_UNSPEC;

		auto endpoint = sockaddr_to_udp_endpoint(storage);
		BOOST_TEST(endpoint.port() == 0);
	}

	BOOST_AUTO_TEST_CASE(sockaddr_to_udp_endpoint_bounded_ipv4)
	{
		sockaddr_in addr4{};
		addr4.sin_family = AF_INET;
		addr4.sin_addr.s_addr = htonl(0xc0000201);
		addr4.sin_port = htons(5353);

		auto endpoint = sockaddr_to_udp_endpoint(
			reinterpret_cast<const sockaddr*>(&addr4), sizeof(addr4));
		BOOST_TEST(endpoint.address().to_string() == "192.0.2.1");
		BOOST_TEST(endpoint.port() == 5353);

		auto short_input = sockaddr_to_udp_endpoint(
			reinterpret_cast<const sockaddr*>(&addr4), sizeof(addr4) - 1);
		BOOST_TEST(short_input.address().is_unspecified());
		BOOST_TEST(short_input.port() == 0);
	}

	BOOST_AUTO_TEST_CASE(sockaddr_to_udp_endpoint_bounded_ipv6)
	{
		sockaddr_in6 addr6{};
		addr6.sin6_family = AF_INET6;
		addr6.sin6_addr.s6_addr[0] = 0x20;
		addr6.sin6_addr.s6_addr[1] = 0x01;
		addr6.sin6_addr.s6_addr[15] = 1;
		addr6.sin6_port = htons(8443);

		auto endpoint = sockaddr_to_udp_endpoint(
			reinterpret_cast<const sockaddr*>(&addr6), sizeof(addr6));
		BOOST_TEST(endpoint.address().to_string() == "2001::1");
		BOOST_TEST(endpoint.port() == 8443);

		auto short_input = sockaddr_to_udp_endpoint(
			reinterpret_cast<const sockaddr*>(&addr6), sizeof(addr6) - 1);
		BOOST_TEST(short_input.address().is_unspecified());
		BOOST_TEST(short_input.port() == 0);
	}
#endif

	BOOST_AUTO_TEST_SUITE_END()

} // namespace
