//
// test_udp_tproxy_flow.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/udp_tproxy_flow.hpp"

#include <boost/test/unit_test.hpp>

#include <cstddef>


namespace {

	using namespace proxy;

#ifdef __linux__
	BOOST_AUTO_TEST_SUITE(udp_tproxy_flow_suite)

	BOOST_AUTO_TEST_CASE(default_field_state)
	{
		udp::endpoint client(net::ip::make_address("127.0.0.1"), 1234);
		udp::endpoint original(net::ip::make_address("8.8.8.8"), 53);

		udp_tproxy_flow flow(client, original);

		BOOST_TEST(flow.client_endp_ == client);
		BOOST_TEST(flow.original_endp_ == original);
		BOOST_TEST(flow.flow_key_ == udp_tproxy_flow_key_hash{}(
			{ client, original }));
		BOOST_TEST(flow.expire_ == 0);
		BOOST_TEST(!flow.closing_);
		BOOST_TEST(!flow.using_connect_udp_);
		BOOST_TEST(!flow.backend_sock_.has_value());
		BOOST_TEST(!flow.relay_sock_.has_value());
		BOOST_TEST(!flow.udp_http_sock_.has_value());
		BOOST_TEST(!flow.notify_timer_.has_value());
		BOOST_TEST(flow.send_queue_.empty());
	}

	BOOST_AUTO_TEST_CASE(flow_key_compares_complete_endpoints)
	{
		udp::endpoint client(net::ip::make_address("127.0.0.1"), 1234);
		udp::endpoint original(net::ip::make_address("8.8.8.8"), 53);

		udp_tproxy_flow_key key{ client, original };
		udp_tproxy_flow_key same{ client, original };
		udp_tproxy_flow_key other_client{
			{ net::ip::make_address("127.0.0.1"), 1235 }, original };
		udp_tproxy_flow_key other_original{
			client, { net::ip::make_address("8.8.8.8"), 54 } };
		udp_tproxy_flow_key reversed{ original, client };

		BOOST_TEST((key == same));
		BOOST_TEST((!(key == other_client)));
		BOOST_TEST((!(key == other_original)));
		BOOST_TEST((!(key == reversed)));

		udp_tproxy_flow_key_hash hash;
		BOOST_TEST(hash(key) == hash(same));
	}

	BOOST_AUTO_TEST_SUITE_END()
#endif

} // namespace
