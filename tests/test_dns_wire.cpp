//
// test_dns_wire.cpp
// ~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/dns_server.hpp"

#include <boost/json.hpp>
#include <boost/test/unit_test.hpp>

#include <cstdint>
#include <string>
#include <vector>


namespace {

	using namespace proxy;

	// 读出 wire-format 报文中的 16 位大端整数字段.
	uint16_t read_u16(const std::string& data, size_t offset)
	{
		return static_cast<uint16_t>(
			(static_cast<uint8_t>(data[offset]) << 8) |
			static_cast<uint8_t>(data[offset + 1]));
	}

	BOOST_AUTO_TEST_SUITE(dns_wire_suite)

	BOOST_AUTO_TEST_CASE(dns_encode_name_encodes_labels)
	{
		std::string expected;
		expected.push_back('\x07');
		expected += "example";
		expected.push_back('\x03');
		expected += "com";
		expected.push_back('\0');

		auto encoded = dns_encode_name("example.com");
		BOOST_TEST(encoded == expected);

		// 末尾的点被忽略, 结果一致.
		BOOST_TEST(dns_encode_name("example.com.") == expected);
	}

	BOOST_AUTO_TEST_CASE(dns_encode_name_root_and_empty)
	{
		BOOST_TEST(dns_encode_name("") == std::string(1, '\0'));
		BOOST_TEST(dns_encode_name(".") == std::string(1, '\0'));
	}

	BOOST_AUTO_TEST_CASE(dns_encode_name_truncates_long_label)
	{
		std::string label(100, 'a');
		auto encoded = dns_encode_name(label);

		// 单标签被截断为 63 字节: 1 字节长度 + 63 字节内容 + 结尾的 0.
		BOOST_TEST(encoded.size() == 65u);
		BOOST_TEST(static_cast<uint8_t>(encoded[0]) == 63u);
	}

	BOOST_AUTO_TEST_CASE(dns_type_name_conversions)
	{
		BOOST_TEST(dns_type_from_string("A") == DNS_TYPE_A);
		BOOST_TEST(dns_type_from_string("AAAA") == DNS_TYPE_AAAA);
		BOOST_TEST(dns_type_from_string("MX") == DNS_TYPE_MX);
		BOOST_TEST(dns_type_from_string("HTTPS") == DNS_TYPE_HTTPS);
		// 无法识别的名字退回 A, 数字字符串按数值解析.
		BOOST_TEST(dns_type_from_string("ZZZ") == DNS_TYPE_A);
		BOOST_TEST(dns_type_from_string("123") == 123);

		BOOST_TEST(dns_type_to_string(DNS_TYPE_A) == "A");
		BOOST_TEST(dns_type_to_string(DNS_TYPE_AAAA) == "AAAA");
		// 未知类型直接输出数字.
		BOOST_TEST(dns_type_to_string(12345) == "12345");
	}

	BOOST_AUTO_TEST_CASE(build_dns_wire_query_layout)
	{
		auto query = build_dns_wire_query("example.com", DNS_TYPE_A);
		// 12 字节头部 + 13 字节 QNAME + 4 字节 QTYPE/QCLASS.
		BOOST_TEST(query.size() == 29u);

		BOOST_TEST(read_u16(query, 2) == 0x0100);  // RD=1
		BOOST_TEST(read_u16(query, 4) == 1);       // QDCOUNT
		BOOST_TEST(read_u16(query, 6) == 0);       // ANCOUNT
		BOOST_TEST(read_u16(query, 10) == 0);      // ARCOUNT

		// CD 标志写入 flags 的 bit4.
		auto cd_query = build_dns_wire_query("example.com", DNS_TYPE_A, true);
		BOOST_TEST(query.size() == cd_query.size());
		BOOST_TEST(read_u16(cd_query, 2) == 0x0110);
	}

	BOOST_AUTO_TEST_CASE(build_dns_wire_query_with_do_bit)
	{
		auto query = build_dns_wire_query("example.com", DNS_TYPE_A, false, true);
		// ARCOUNT = 1, 并附加 11 字节的 OPT 伪记录.
		BOOST_TEST(read_u16(query, 10) == 1);
		BOOST_TEST(query.size() == 29u + 11u);
	}

	BOOST_AUTO_TEST_CASE(dns_parse_query_round_trip)
	{
		auto query = build_dns_wire_query("example.com", DNS_TYPE_MX);

		std::string name;
		uint16_t type = 0;
		BOOST_TEST(dns_parse_query(query, name, type));
		BOOST_TEST(name == "example.com");
		BOOST_TEST(type == DNS_TYPE_MX);

		// 过短的报文解析失败.
		BOOST_TEST(!dns_parse_query(std::string(11, '\0'), name, type));
	}

	BOOST_AUTO_TEST_CASE(dns_query_flags_extracts_cd_and_do)
	{
		bool cd = false;
		bool do_bit = false;

		dns_query_flags(build_dns_wire_query("a.com", DNS_TYPE_A), cd, do_bit);
		BOOST_TEST(!cd);
		BOOST_TEST(!do_bit);

		dns_query_flags(
			build_dns_wire_query("a.com", DNS_TYPE_A, true, true), cd, do_bit);
		BOOST_TEST(cd);
		BOOST_TEST(do_bit);
	}

	BOOST_AUTO_TEST_CASE(dns_parse_name_follows_compression_pointer)
	{
		std::string message(12, '\0');
		message += dns_encode_name("example.com"); // 偏移 12, 13 字节
		message.push_back(static_cast<char>(0xC0)); // 指向偏移 12
		message.push_back(0x0C);

		const char* start = message.data();
		const char* p = start + 25;
		auto [name, next] = dns_parse_name(p, start + message.size(), start);

		BOOST_TEST(name == "example.com.");
		BOOST_TEST(next == p + 2);
	}

	BOOST_AUTO_TEST_CASE(dns_rdata_to_string_formats_a_record)
	{
		const char data[] = { '\x0a', '\0', '\0', '\x01' };
		BOOST_TEST(dns_rdata_to_string(
			DNS_TYPE_A, 4, data, data + 4, data) == "10.0.0.1");
	}

	BOOST_AUTO_TEST_CASE(dns_build_response_echoes_question)
	{
		auto query = build_dns_wire_query("example.com", DNS_TYPE_A);
		auto response = dns_build_response(query, 0, {});

		// 回显 ID 与问题, 无应答记录.
		BOOST_TEST(read_u16(response, 0) == read_u16(query, 0));
		BOOST_TEST(read_u16(response, 2) == 0x8180); // QR=1, RD=1, RA=1
		BOOST_TEST(read_u16(response, 4) == 1);      // QDCOUNT
		BOOST_TEST(read_u16(response, 6) == 0);      // ANCOUNT
		BOOST_TEST(response.size() == query.size());
	}

	BOOST_AUTO_TEST_CASE(dns_build_response_carries_answers)
	{
		auto query = build_dns_wire_query("example.com", DNS_TYPE_A);

		const std::string rdata{ '\x0a', '\x00', '\x00', '\x01' };
		std::vector<dns_answer> answers{
			{ "example.com.", DNS_TYPE_A, 60, rdata }
		};

		auto response = dns_build_response(query, 0, answers);
		BOOST_TEST(read_u16(response, 6) == 1); // ANCOUNT
		// 头部 12 + 问题 17 + 应答记录 13 + 10 + 4.
		BOOST_TEST(response.size() == 56u);
	}

	BOOST_AUTO_TEST_CASE(dns_build_response_sets_rcode)
	{
		auto query = build_dns_wire_query("example.com", DNS_TYPE_A);

		auto nxdomain = dns_build_response(query, 3, {});
		BOOST_TEST((read_u16(nxdomain, 2) & 0x000F) == 3);

		auto servfail = dns_build_response(query, 2, {});
		BOOST_TEST((read_u16(servfail, 2) & 0x000F) == 2);

		// 过短的查询报文返回空串.
		BOOST_TEST(dns_build_response(std::string(11, '\0'), 0, {}).empty());
	}

	BOOST_AUTO_TEST_CASE(dns_cache_key_includes_flags)
	{
		BOOST_TEST(dns_cache_key("a.com", DNS_TYPE_A, false, false) == "a.com/1/0");
		BOOST_TEST(dns_cache_key("a.com", DNS_TYPE_A, true, false) == "a.com/1/1");
		BOOST_TEST(dns_cache_key("a.com", DNS_TYPE_A, false, true) == "a.com/1/2");
		BOOST_TEST(dns_cache_key("a.com", DNS_TYPE_A, true, true) == "a.com/1/3");
	}

	BOOST_AUTO_TEST_CASE(dns_strip_and_set_id)
	{
		auto query = build_dns_wire_query("example.com", DNS_TYPE_A);
		auto response = dns_build_response(query, 0, {});

		auto stripped = dns_strip_id(response);
		BOOST_TEST(stripped.size() == response.size());
		BOOST_TEST(stripped[0] == '\0');
		BOOST_TEST(stripped[1] == '\0');

		auto restored = dns_set_id(stripped, 0xABCD);
		BOOST_TEST(static_cast<uint8_t>(restored[0]) == 0xAB);
		BOOST_TEST(static_cast<uint8_t>(restored[1]) == 0xCD);

		// 过短输入原样返回.
		BOOST_TEST(dns_strip_id("a") == "a");
		BOOST_TEST(dns_set_id("a", 1) == "a");
	}

	BOOST_AUTO_TEST_CASE(dns_cacheable_skips_error_rcodes)
	{
		auto query = build_dns_wire_query("example.com", DNS_TYPE_A);

		BOOST_TEST(dns_cacheable(dns_build_response(query, 0, {})));
		BOOST_TEST(!dns_cacheable(dns_build_response(query, 2, {})));
		BOOST_TEST(!dns_cacheable(dns_build_response(query, 3, {})));
		BOOST_TEST(!dns_cacheable("ab"));
	}

	BOOST_AUTO_TEST_CASE(dns_response_to_json_shape)
	{
		auto query = build_dns_wire_query("example.com", DNS_TYPE_A);

		const std::string rdata{ '\x0a', '\x00', '\x00', '\x01' };
		std::vector<dns_answer> answers{
			{ "example.com.", DNS_TYPE_A, 60, rdata }
		};

		auto response = dns_build_response(query, 0, answers);
		auto json = dns_response_to_json(response, "example.com", DNS_TYPE_A);

		auto value = boost::json::parse(json);
		BOOST_TEST(value.at("Status").as_int64() == 0);
		BOOST_TEST(value.at("Question").as_array().at(0).at("name").as_string() ==
			"example.com.");
		BOOST_TEST(value.at("Question").as_array().at(0).at("type").as_int64() ==
			DNS_TYPE_A);
		BOOST_TEST(value.at("Answer").as_array().at(0).at("data").as_string() ==
			"10.0.0.1");
	}

	BOOST_AUTO_TEST_CASE(dns_response_to_json_rejects_short_response)
	{
		auto json = dns_response_to_json("short", "example.com", DNS_TYPE_A);
		auto value = boost::json::parse(json);
		BOOST_TEST(value.at("Status").as_int64() == -1);
	}

	BOOST_AUTO_TEST_SUITE_END()

} // namespace
