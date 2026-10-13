//
// test_socks_error_code.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/socks_error_code.hpp"

#include <boost/test/unit_test.hpp>

#include <string>


namespace {

	using namespace proxy;

	BOOST_AUTO_TEST_SUITE(socks_error_code_suite)

	BOOST_AUTO_TEST_CASE(category_is_stable_singleton)
	{
		BOOST_TEST(std::string(error_category().name()) == "SOCKS");
		BOOST_TEST(&error_category() == &error_category());
	}

	BOOST_AUTO_TEST_CASE(make_error_code_round_trip)
	{
		auto ec = errc::make_error_code(errc::socks_connection_refused);

		BOOST_TEST(ec.value() ==
			static_cast<int>(errc::socks_connection_refused));
		bool same_category = ec.category() == error_category();
		BOOST_TEST(same_category);
		BOOST_TEST(!!ec);
		BOOST_TEST(ec == errc::socks_connection_refused);
	}

	BOOST_AUTO_TEST_CASE(known_codes_have_specific_messages)
	{
		BOOST_TEST(errc::make_error_code(errc::socks_unsupported_version).message() ==
			"SOCKS unsupported version");
		BOOST_TEST(errc::make_error_code(errc::socks_username_required).message() ==
			"SOCKS username required");
		BOOST_TEST(errc::make_error_code(errc::socks_authentication_error).message() ==
			"SOCKS authentication error");
		BOOST_TEST(errc::make_error_code(errc::socks_network_unreachable).message() ==
			"SOCKS network unreachable");
		BOOST_TEST(errc::make_error_code(errc::socks_host_unreachable).message() ==
			"SOCKS host unreachable");
		BOOST_TEST(errc::make_error_code(errc::socks_ttl_expired).message() ==
			"SOCKS TTL expired");
		BOOST_TEST(errc::make_error_code(errc::socks_command_not_supported).message() ==
			"SOCKS command not supported");
		BOOST_TEST(errc::make_error_code(errc::socks_address_type_not_supported).message() ==
			"SOCKS Address type not supported");
		BOOST_TEST(errc::make_error_code(errc::socks_no_identd).message() ==
			"SOCKS no identd running");
		BOOST_TEST(errc::make_error_code(errc::socks_request_rejected_or_failed).message() ==
			"SOCKS request rejected or failed");
	}

	BOOST_AUTO_TEST_CASE(unknown_value_falls_back_to_generic_message)
	{
		boost::system::error_code ec(1, error_category());
		BOOST_TEST(ec.message() == "SOCKS Unknown PROXY error");
	}

	BOOST_AUTO_TEST_SUITE_END()

} // namespace
