//
// Copyright (c) 2023-2025 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mqtt5/error.hpp>
#include <boost/mqtt5/reason_codes.hpp>

#include <boost/test/unit_test.hpp>

#include <iosfwd>
#include <string>
#include <vector>

using namespace boost::mqtt5;

BOOST_AUTO_TEST_SUITE(error/*, *boost::unit_test::disabled()*/)

struct client_error_codes {
    const client::client_ec_category& cat = client::get_error_code_category();

    const std::vector<client::error> ecs = {
        client::error::malformed_packet,
        client::error::packet_too_large,
        client::error::session_expired,
        client::error::pid_overrun,
        client::error::invalid_topic,
        client::error::qos_not_supported,
        client::error::retain_not_available,
        client::error::topic_alias_maximum_reached,
        client::error::wildcard_subscription_not_available,
        client::error::subscription_identifier_not_available,
        client::error::shared_subscription_not_available
    };
};

BOOST_FIXTURE_TEST_CASE(client_ec_to_string, client_error_codes) {
    // Ensure that all branches of the switch/case are covered
    BOOST_TEST(cat.name());

    constexpr auto default_output = "Unknown client error";
    for (auto ec : ecs)
        BOOST_TEST(cat.message(static_cast<int>(ec)) != default_output);

    // default branch
    BOOST_TEST(cat.message(1) == default_output);
}

BOOST_FIXTURE_TEST_CASE(client_ec_to_stream, client_error_codes) {
    for (auto ec : ecs) {
        std::ostringstream stream;
        stream << ec;
        std::string expected = std::string(cat.name()) + ":" +
            std::to_string(static_cast<int>(ec));
        BOOST_TEST(stream.str() == expected);
    }
}

using namespace reason_codes;
struct client_reason_codes {
    const std::vector<reason_code> rcs = {
        empty, success, normal_disconnection,
        granted_qos_0, granted_qos_1, granted_qos_2,
        disconnect_with_will_message, no_matching_subscribers,
        no_subscription_existed, continue_authentication, reauthenticate,
        unspecified_error, malformed_packet, protocol_error,
        implementation_specific_error, unsupported_protocol_version,
        client_identifier_not_valid,bad_username_or_password,
        not_authorized, server_unavailable, server_busy, banned,
        server_shutting_down, bad_authentication_method, keep_alive_timeout,
        session_taken_over, topic_filter_invalid, topic_name_invalid,
        packet_identifier_in_use, packet_identifier_not_found, receive_maximum_exceeded,
        topic_alias_invalid, packet_too_large, message_rate_too_high,
        quota_exceeded, administrative_action, payload_format_invalid,
        retain_not_supported, qos_not_supported, use_another_server,
        server_moved, shared_subscriptions_not_supported, connection_rate_exceeded,
        maximum_connect_time, subscription_ids_not_supported,
        wildcard_subscriptions_not_supported
    };
};

BOOST_FIXTURE_TEST_CASE(reason_code_to_string, client_reason_codes) {
    // Ensure that all branches of the switch/case are covered
    BOOST_TEST(rcs.size() == 46u);

    constexpr auto default_output = "Invalid reason code";
    for (const auto& rc: rcs) 
        BOOST_TEST(rc.message() != "Invalid reason code");

    // default branch
    BOOST_TEST(
        reason_code(0x05, reason_codes::category::suback).message() == default_output
    );
}

BOOST_FIXTURE_TEST_CASE(reason_code_to_stream, client_reason_codes) {
    for (const auto& rc : rcs) {
        std::ostringstream stream;
        stream << rc;
        BOOST_TEST(stream.str() == rc.message());
    }
}

BOOST_AUTO_TEST_SUITE_END();
