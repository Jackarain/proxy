//
// Copyright (c) 2023-2025 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mqtt5/mqtt_client.hpp>
#include <boost/mqtt5/types.hpp>

#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/test/unit_test.hpp>

#include <chrono>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "test_common/message_exchange.hpp"
#include "test_common/packet_util.hpp"
#include "test_common/test_authenticators.hpp"
#include "test_common/test_broker.hpp"
#include "test_common/test_stream.hpp"

using namespace boost::mqtt5;

BOOST_AUTO_TEST_SUITE(re_authentication/*, *boost::unit_test::disabled()*/)

struct shared_test_data {
    error_code success {};
    error_code fail = asio::error::not_connected;

    const std::string connect = encoders::encode_connect(
        "", std::nullopt, std::nullopt, 60, false, init_connect_props(), std::nullopt
    );
    const std::string connack = encoders::encode_connack(
        true, reason_codes::success.value(), {}
    );

    const std::string auth_challenge = encoders::encode_auth(
        reason_codes::reauthenticate.value(), init_auth_props()
    );
    const std::string auth_response = encoders::encode_auth(
        reason_codes::continue_authentication.value(), init_auth_props()
    );
    const std::string auth_success = encoders::encode_auth(
        reason_codes::success.value(), init_auth_props()
    );

    connect_props init_connect_props() {
        connect_props cprops;
        cprops[prop::authentication_method] = "method";
        cprops[prop::authentication_data] = "";
        return cprops;
    }

    auth_props init_auth_props() {
        auth_props aprops;
        aprops[prop::authentication_method] = "method";
        aprops[prop::authentication_data] = "";
        return aprops;
    }
};

using test::after;
using namespace std::chrono_literals;

template <typename Authenticator = std::monostate>
void run_test(
    test::msg_exchange broker_side, Authenticator&& authenticator = {}
) {
    asio::io_context ioc;
    auto executor = ioc.get_executor();
    auto& broker = asio::make_service<test::test_broker>(
        ioc, executor, std::move(broker_side)
    );

    using client_type = mqtt_client<test::test_stream>;
    client_type c(executor);
    c.brokers("127.0.0.1");

    if constexpr (!std::is_same_v<Authenticator, std::monostate>)
        c.authenticator(std::forward<Authenticator>(authenticator));

    c.async_run(asio::detached);

    asio::steady_timer timer(c.get_executor());
    // wait until the connection is established
    timer.expires_after(20ms);
    timer.async_wait([&](error_code) {
        c.re_authenticate();

        timer.expires_after(150ms);
        timer.async_wait([&c](error_code) { c.cancel(); });
    });

    ioc.run();
    BOOST_TEST(broker.received_all_expected());
}

BOOST_FIXTURE_TEST_CASE(successful_re_auth, shared_test_data) {
    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(0ms))
            .reply_with(connack, after(1ms))
        .expect(auth_challenge)
            .complete_with(success, after(2ms))
            .reply_with(auth_success, after(4ms));

    run_test(std::move(broker_side), test::test_authenticator());
}

BOOST_FIXTURE_TEST_CASE(successful_re_auth_multi_step, shared_test_data) {
    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(0ms))
            .reply_with(connack, after(1ms))
        .expect(auth_challenge)
            .complete_with(success, after(2ms))
            .reply_with(auth_response, after(4ms))
        .expect(auth_response)
            .complete_with(success, after(2ms))
            .reply_with(auth_success, after(4ms));

    run_test(std::move(broker_side), test::test_authenticator());
}

BOOST_FIXTURE_TEST_CASE(malformed_auth_rc, shared_test_data) {
    auto disconnect = encoders::encode_disconnect(
        reason_codes::malformed_packet.value(),
        test::dprops_with_reason_string("Malformed AUTH received: bad reason code")
    );
    auto malformed_auth = encoders::encode_auth(
        reason_codes::administrative_action.value(), init_auth_props()
    );

    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(0ms))
            .reply_with(connack, after(1ms))
        .expect(auth_challenge)
            .complete_with(success, after(2ms))
            .reply_with(malformed_auth, after(4ms))
        .expect(disconnect)
            .complete_with(success, after(2ms));

    run_test(std::move(broker_side), test::test_authenticator());
}

BOOST_FIXTURE_TEST_CASE(mismatched_auth_method, shared_test_data) {
    auth_props aprops;
    aprops[prop::authentication_method] = "wrong method";

    auto mismatched_auth_response = encoders::encode_auth(
        reason_codes::continue_authentication.value(), aprops
    );

    auto disconnect = encoders::encode_disconnect(
        reason_codes::protocol_error.value(),
        test::dprops_with_reason_string("Malformed AUTH received: wrong authentication method")
    );

    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(0ms))
            .reply_with(connack, after(1ms))
        .expect(auth_challenge)
            .complete_with(success, after(2ms))
            .reply_with(mismatched_auth_response, after(4ms))
        .expect(disconnect)
            .complete_with(success, after(2ms));

    run_test(std::move(broker_side), test::test_authenticator());
}

BOOST_FIXTURE_TEST_CASE(malformed_auth_received, shared_test_data) {
    auto malformed_auth = std::string { -16, 3, 24, 15, 1, 0 };
    auto disconnect = encoders::encode_disconnect(
        reason_codes::malformed_packet.value(),
        test::dprops_with_reason_string("Malformed AUTH received: cannot decode")
    );

    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(0ms))
            .reply_with(connack, after(1ms))
        .expect(auth_challenge)
            .complete_with(success, after(2ms))
            .reply_with(malformed_auth, after(4ms))
        .expect(disconnect)
            .complete_with(success, after(2ms));

    run_test(std::move(broker_side), test::test_authenticator());
}

BOOST_FIXTURE_TEST_CASE(async_auth_fail, shared_test_data) {
    auto disconnect = encoders::encode_disconnect(
        reason_codes::unspecified_error.value(),
        test::dprops_with_reason_string("Re-authentication: authentication fail")
    );

    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(0ms))
            .reply_with(connack, after(1ms))
        .expect(auth_challenge)
            .complete_with(success, after(2ms))
            .reply_with(auth_response, after(4ms))
        .expect(disconnect)
            .complete_with(success, after(2ms));

    run_test(
        std::move(broker_side),
        test::fail_test_authenticator<auth_step_e::server_challenge>()
    );
}

BOOST_FIXTURE_TEST_CASE(unexpected_auth, shared_test_data) {
    auto connect_no_auth = encoders::encode_connect(
        "", std::nullopt, std::nullopt, 60, false, {}, std::nullopt
    );
    auto disconnect = encoders::encode_disconnect(
        reason_codes::protocol_error.value(),
        test::dprops_with_reason_string("Unexpected AUTH received")
    );

    test::msg_exchange broker_side;
    broker_side
        .expect(connect_no_auth)
            .complete_with(success, after(0ms))
            .reply_with(connack, after(1ms))
        .send(auth_challenge, after(50ms))
        .expect(disconnect)
            .complete_with(success, after(2ms));

    run_test(
        std::move(broker_side)
    );
}

BOOST_FIXTURE_TEST_CASE(re_auth_without_authenticator, shared_test_data) {
    auto connect_no_auth = encoders::encode_connect(
        "", std::nullopt, std::nullopt, 60, false, {}, std::nullopt
    );

    test::msg_exchange broker_side;
    broker_side
        .expect(connect_no_auth)
            .complete_with(success, after(0ms))
            .reply_with(connack, after(1ms));

    run_test(std::move(broker_side));
}

BOOST_AUTO_TEST_SUITE_END();
