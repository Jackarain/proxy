//
// test_authority.cpp
// ~~~~~~~~~~~~~~~~~~

#include "proxy/authority.hpp"

#include <boost/test/unit_test.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>


namespace {

	namespace authority = proxy::authority;

	struct parse_result
	{
		bool ok { false };
		std::string host;
		uint16_t port { 0 };
	};

	parse_result parse(std::string_view value)
	{
		parse_result result;
		result.ok = authority::parse_authority(value, result.host, result.port);
		return result;
	}

	BOOST_AUTO_TEST_SUITE(authority_suite)

	BOOST_AUTO_TEST_CASE(parses_domain_ipv4_and_port_boundaries)
	{
		auto domain = parse("example.com:443");
		BOOST_TEST(domain.ok);
		BOOST_TEST(domain.host == "example.com");
		BOOST_TEST(domain.port == 443);

		auto minimum = parse("127.0.0.1:1");
		BOOST_TEST(minimum.ok);
		BOOST_TEST(minimum.host == "127.0.0.1");
		BOOST_TEST(minimum.port == 1);

		auto maximum = parse("localhost:65535");
		BOOST_TEST(maximum.ok);
		BOOST_TEST(maximum.host == "localhost");
		BOOST_TEST(maximum.port == 65535);
	}

	BOOST_AUTO_TEST_CASE(trims_surrounding_space_and_tab)
	{
		auto result = parse(" \texample.com:8080\t ");
		BOOST_TEST(result.ok);
		BOOST_TEST(result.host == "example.com");
		BOOST_TEST(result.port == 8080);
	}

	BOOST_AUTO_TEST_CASE(parses_bracketed_ipv6_with_scope_id)
	{
		auto global = parse("[2001:db8::1]:8443");
		BOOST_TEST(global.ok);
		BOOST_TEST(global.host == "2001:db8::1");
		BOOST_TEST(global.port == 8443);

		auto loopback = parse("[::1]:53");
		BOOST_TEST(loopback.ok);
		BOOST_TEST(loopback.host == "::1");
		BOOST_TEST(loopback.port == 53);

		auto scoped = parse("[fe80::1%eth0]:546");
		BOOST_TEST(scoped.ok);
		BOOST_TEST(scoped.host == "fe80::1%eth0");
		BOOST_TEST(scoped.port == 546);
	}

	BOOST_AUTO_TEST_CASE(rejects_malformed_or_ambiguous_authorities)
	{
		const char* values[] = {
			"", " ", "example.com", ":80", "example.com:",
			"example.com:0", "example.com:65536", "example.com:-1",
			"example.com:+80", "example.com:80x", "2001:db8::1:443",
			"[::1]", "[::1]:", "[::1]:0", "[::1]:65536", "[::1]x:80",
			"[example.com]:80", "[]:80", "[::1", "[::1]:80]",
			"example.com:80/path", "user@example.com:80",
			"example.com:80?x", "example.com:80#x", "exa mple.com:80",
			"exa\tmple.com:80", "example.com:80\\path"
		};

		for (const char* value : values)
		{
			BOOST_TEST_CONTEXT(value)
				BOOST_TEST(!parse(value).ok);
		}
	}

	BOOST_AUTO_TEST_CASE(enforces_authority_size_limit)
	{
		std::string allowed(authority::max_authority_size - 3, 'a');
		allowed.append(":80");
		BOOST_TEST(allowed.size() == authority::max_authority_size);
		BOOST_TEST(parse(allowed).ok);

		std::string rejected(authority::max_authority_size - 2, 'a');
		rejected.append(":80");
		BOOST_TEST(rejected.size() == authority::max_authority_size + 1);
		BOOST_TEST(!parse(rejected).ok);
	}

	BOOST_AUTO_TEST_CASE(validates_host_characters)
	{
		for (char c : std::string("aZ09.-_:%"))
			BOOST_TEST_CONTEXT(c)
				BOOST_TEST(authority::valid_host_char(c));

		const char invalid[] = {
			'\0', '\x1f', ' ', '\x7f', '/', '@', '?', '#', '[', ']'
		};
		for (char c : invalid)
			BOOST_TEST_CONTEXT(static_cast<int>(c))
				BOOST_TEST(!authority::valid_host_char(c));
	}

	BOOST_AUTO_TEST_CASE(formats_ipv4_domain_and_ipv6)
	{
		BOOST_TEST(authority::format_authority("example.com", 443) ==
			"example.com:443");
		BOOST_TEST(authority::format_authority("127.0.0.1", 53) ==
			"127.0.0.1:53");
		BOOST_TEST(authority::format_authority("2001:db8::1", 8443) ==
			"[2001:db8::1]:8443");
		BOOST_TEST(authority::format_authority("fe80::1%eth0", 546) ==
			"[fe80::1%eth0]:546");
	}

	BOOST_AUTO_TEST_CASE(format_and_parse_round_trip)
	{
		const std::pair<const char*, uint16_t> cases[] = {
			{ "example.com", 443 },
			{ "127.0.0.1", 53 },
			{ "2001:db8::1", 8443 },
			{ "fe80::1%eth0", 546 }
		};

		for (const auto& [host, port] : cases)
		{
			auto result = parse(authority::format_authority(host, port));
			BOOST_TEST_CONTEXT(host)
			{
				BOOST_TEST(result.ok);
				BOOST_TEST(result.host == host);
				BOOST_TEST(result.port == port);
			}
		}
	}

	BOOST_AUTO_TEST_SUITE_END()

} // namespace
