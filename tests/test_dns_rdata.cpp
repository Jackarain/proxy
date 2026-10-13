//
// test_dns_rdata.cpp
// ~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/dns_server.hpp"

#include <boost/test/unit_test.hpp>

#include <cstdint>
#include <string>


namespace {

	using namespace proxy;

	void append_u16(std::string& out, uint16_t value)
	{
		out.push_back(static_cast<char>((value >> 8) & 0xff));
		out.push_back(static_cast<char>(value & 0xff));
	}

	void append_u32(std::string& out, uint32_t value)
	{
		out.push_back(static_cast<char>((value >> 24) & 0xff));
		out.push_back(static_cast<char>((value >> 16) & 0xff));
		out.push_back(static_cast<char>((value >> 8) & 0xff));
		out.push_back(static_cast<char>(value & 0xff));
	}

	// 以自身为报文起点解析 RDATA.
	std::string rdata_to_string(uint16_t type, const std::string& rdata)
	{
		return dns_rdata_to_string(type,
			static_cast<uint16_t>(rdata.size()),
			rdata.data(), rdata.data() + rdata.size(), rdata.data());
	}

	BOOST_AUTO_TEST_SUITE(dns_rdata_suite)

	BOOST_AUTO_TEST_CASE(aaaa_record)
	{
		// 2001:db8::1
		std::string rdata;
		append_u16(rdata, 0x2001);
		append_u16(rdata, 0x0db8);
		for (int i = 0; i < 12; i++)
			rdata.push_back('\0');
		rdata[rdata.size() - 1] = 0x01;

		BOOST_TEST(rdata_to_string(DNS_TYPE_AAAA, rdata) == "2001:db8::1");
	}

	BOOST_AUTO_TEST_CASE(name_based_records)
	{
		BOOST_TEST(rdata_to_string(DNS_TYPE_CNAME,
			dns_encode_name("target.example.com")) == "target.example.com.");
		BOOST_TEST(rdata_to_string(DNS_TYPE_NS,
			dns_encode_name("ns1.example.com")) == "ns1.example.com.");
		BOOST_TEST(rdata_to_string(DNS_TYPE_PTR,
			dns_encode_name("host.example.com")) == "host.example.com.");
	}

	BOOST_AUTO_TEST_CASE(mx_record)
	{
		std::string rdata;
		append_u16(rdata, 10);
		rdata += dns_encode_name("mail.example.com");

		BOOST_TEST(rdata_to_string(DNS_TYPE_MX, rdata) == "10 mail.example.com.");
	}

	BOOST_AUTO_TEST_CASE(txt_record_joins_character_strings)
	{
		std::string rdata;
		rdata.push_back(3);
		rdata += "abc";
		rdata.push_back(2);
		rdata += "de";

		BOOST_TEST(rdata_to_string(DNS_TYPE_TXT, rdata) == "abc\nde");
	}

	BOOST_AUTO_TEST_CASE(soa_record)
	{
		std::string rdata = dns_encode_name("ns.example.com");
		rdata += dns_encode_name("hostmaster.example.com");
		append_u32(rdata, 1);
		append_u32(rdata, 2);
		append_u32(rdata, 3);
		append_u32(rdata, 4);
		append_u32(rdata, 5);

		BOOST_TEST(rdata_to_string(DNS_TYPE_SOA, rdata) ==
			"ns.example.com. hostmaster.example.com. 1 2 3 4 5");
	}

	BOOST_AUTO_TEST_CASE(srv_record)
	{
		std::string rdata;
		append_u16(rdata, 1);
		append_u16(rdata, 2);
		append_u16(rdata, 443);
		rdata += dns_encode_name("svc.example.com");

		BOOST_TEST(rdata_to_string(DNS_TYPE_SRV, rdata) ==
			"1 2 443 svc.example.com.");
	}

	BOOST_AUTO_TEST_CASE(caa_record)
	{
		std::string rdata;
		rdata.push_back(0);   // flags
		rdata.push_back(5);   // tag length
		rdata += "issue";
		rdata += "ca.example.com";

		BOOST_TEST(rdata_to_string(DNS_TYPE_CAA, rdata) ==
			"0 issue \"ca.example.com\"");
	}

	BOOST_AUTO_TEST_CASE(https_record_with_alpn_svcparam)
	{
		std::string rdata;
		append_u16(rdata, 1);          // priority
		rdata.push_back('\0');        // target: root
		append_u16(rdata, 1);          // key: alpn
		append_u16(rdata, 3);          // length
		rdata.push_back(2);           // alpn length
		rdata += "h2";

		BOOST_TEST(rdata_to_string(DNS_TYPE_HTTPS, rdata) == "1 . alpn=h2");
	}

	BOOST_AUTO_TEST_CASE(unknown_type_dumps_hex)
	{
		const std::string rdata{ '\x01', '\xab', '\xff' };
		BOOST_TEST(rdata_to_string(9999, rdata) == "01abff");
	}

	BOOST_AUTO_TEST_CASE(truncated_rdata_returns_raw_bytes)
	{
		const std::string rdata{ 'a', 'b', 'c' };
		// rdlength 超出 end 时按剩余长度返回原始字节.
		auto result = dns_rdata_to_string(DNS_TYPE_A, 4,
			rdata.data(), rdata.data() + 2, rdata.data());
		BOOST_TEST(result == "ab");
	}

	BOOST_AUTO_TEST_CASE(parse_name_plain_sequence)
	{
		auto encoded = dns_encode_name("www.example.com");
		const char* start = encoded.data();
		auto [name, next] = dns_parse_name(
			start, start + encoded.size(), start);

		BOOST_TEST(name == "www.example.com.");
		BOOST_TEST(next == start + encoded.size());
	}

	BOOST_AUTO_TEST_CASE(parse_name_stops_on_pointer_loop)
	{
		const char loop[] = { static_cast<char>(0xC0), 0x00 };
		const char* start = loop;

		auto [name, next] = dns_parse_name(start, start + sizeof(loop), start);

		BOOST_TEST(name.empty());
		BOOST_TEST(next == start + 2);
	}

	BOOST_AUTO_TEST_SUITE_END()

} // namespace
