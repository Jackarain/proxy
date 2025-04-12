//
// Copyright (c) 2023-2025 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mqtt5/logger_traits.hpp>
#include <boost/mqtt5/types.hpp>

#include <boost/mqtt5/detail/internal_types.hpp>
#include <boost/mqtt5/detail/log_invoke.hpp>

#include <boost/mqtt5/impl/connect_op.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/test/unit_test.hpp>

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "test_common/test_authenticators.hpp"
#include "test_common/test_stream.hpp"

using namespace boost::mqtt5;

BOOST_AUTO_TEST_SUITE(connect_op/*, *boost::unit_test::disabled()*/)

struct shared_test_data {
    error_code success {};
    error_code fail = asio::error::not_connected;

    const std::string connect = encoders::encode_connect(
        "", std::nullopt, std::nullopt, 60, false, {}, std::nullopt
    );
    const std::string connack = encoders::encode_connack(
        true, reason_codes::success.value(), {}
    );
};

using test::after;
using namespace std::chrono_literals;

void run_unit_test(
    detail::mqtt_ctx mqtt_ctx, test::msg_exchange broker_side,
    asio::any_completion_handler<void(error_code)> op_handler
) {
    constexpr int expected_handlers_called = 1;
    int handlers_called = 0;

    asio::io_context ioc;
    auto executor = ioc.get_executor();
    auto& broker = asio::make_service<test::test_broker>(
        ioc, executor, std::move(broker_side)
    );
    test::test_stream stream(executor);

    authority_path ap;
    auto eps = asio::ip::tcp::resolver(executor).resolve("127.0.0.1", "");

    auto handler = [&handlers_called, h = std::move(op_handler)](error_code ec) mutable {
        handlers_called++;
        std::move(h)(ec);
    };

    detail::log_invoke<noop_logger> d;
    detail::connect_op<test::test_stream, noop_logger>(
        stream, mqtt_ctx, d, std::move(handler)
    ).perform(*std::begin(eps), std::move(ap));

    ioc.run_for(1s);
    BOOST_TEST(handlers_called == expected_handlers_called);
    BOOST_TEST(broker.received_all_expected());
}

void run_unit_test(
    test::msg_exchange broker_side,
    asio::any_completion_handler<void(error_code)> op_handler
) {
    return run_unit_test(
        detail::mqtt_ctx(), std::move(broker_side), std::move(op_handler)
    );
}

BOOST_FIXTURE_TEST_CASE(successfully_connect, shared_test_data) {
    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(2ms))
            .reply_with(connack, after(4ms));

    auto handler = [&](error_code ec) {
        BOOST_TEST(ec == success);
    };

    run_unit_test(std::move(broker_side), std::move(handler));
}

BOOST_FIXTURE_TEST_CASE(connack_with_fail_rc, shared_test_data) {
    auto denied_connack = encoders::encode_connack(
        true, reason_codes::bad_username_or_password.value(), {}
    );

    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(2ms))
            .reply_with(denied_connack, after(4ms));

    auto handler = [&](error_code ec) {
        BOOST_TEST(ec == asio::error::try_again);
    };

    run_unit_test(std::move(broker_side), std::move(handler));
}

BOOST_FIXTURE_TEST_CASE(fail_to_send_connect, shared_test_data) {
    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
        .complete_with(fail, after(2ms));

    auto handler = [&](error_code ec) {
        BOOST_TEST(ec == fail);
    };

    run_unit_test(std::move(broker_side), std::move(handler));
}

BOOST_FIXTURE_TEST_CASE(receive_wrong_packet, shared_test_data) {
    // packets
    auto unexpected_packet = encoders::encode_puback(1, uint8_t(0x00), {});

    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(2ms))
            .reply_with(unexpected_packet, after(3ms));

    auto handler = [&](error_code ec) {
        BOOST_TEST(ec == asio::error::try_again);
    };

    run_unit_test(std::move(broker_side), std::move(handler));
}

BOOST_FIXTURE_TEST_CASE(malformed_connack_varlen, shared_test_data) {
    // packets
    auto malformed_connack = std::string({ 0x20, -1 /* 0xFF */, -1, -1, -1 });

    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(2ms))
            .reply_with(malformed_connack, after(3ms));

    auto handler = [&](error_code ec) {
        BOOST_TEST(ec == asio::error::try_again);
    };

    run_unit_test(std::move(broker_side), std::move(handler));
}

BOOST_FIXTURE_TEST_CASE(malformed_connack_rc, shared_test_data) {
    // packets
    auto malformed_connack = encoders::encode_connack(true, uint8_t(0x04), {});

    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
        .complete_with(success, after(2ms))
        .reply_with(malformed_connack, after(3ms));

    auto handler = [&](error_code ec) {
        BOOST_TEST(ec == client::error::malformed_packet);
    };

    run_unit_test(std::move(broker_side), std::move(handler));
}

BOOST_FIXTURE_TEST_CASE(fail_reading_connack_payload, shared_test_data) {
    // packets
    connack_props cprops;
    cprops[prop::reason_string] = std::string(256, 'a');

    auto big_connack = encoders::encode_connack(
        true, uint8_t(0x00), cprops
    );

    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(2ms))
        .send(big_connack.substr(0, 5), after(5ms))
        .send(fail, after(7ms));

    auto handler = [&](error_code ec) {
        BOOST_TEST(ec == fail);
    };

    run_unit_test(std::move(broker_side), std::move(handler));
}

BOOST_FIXTURE_TEST_CASE(receive_unexpected_auth, shared_test_data) {
    auth_props aprops;
    aprops[prop::authentication_method] = "method";
    aprops[prop::authentication_data] = "data";

    auto auth = encoders::encode_auth(uint8_t(0x19), aprops);

    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(2ms))
        .send(auth, after(5ms));

    auto handler = [&](error_code ec) {
        BOOST_TEST(ec == client::error::malformed_packet);
    };

    run_unit_test(std::move(broker_side), std::move(handler));
}

// enhanced auth

struct shared_test_auth_data {
    error_code success {};
    error_code fail = asio::error::not_connected;

    const std::string connect = encoders::encode_connect(
        "", std::nullopt, std::nullopt, 60, false, init_connect_props(), std::nullopt
    );

    const std::string connack = encoders::encode_connack(
        true, reason_codes::success.value(), {}
    );

    const std::string auth_challenge = encoders::encode_auth(
        reason_codes::continue_authentication.value(), init_auth_props()
    );

    const std::string auth_response = auth_challenge;

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

BOOST_FIXTURE_TEST_CASE(successful_auth, shared_test_auth_data) {
    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(2ms))
            .reply_with(auth_challenge, after(3ms))
        .expect(auth_response)
            .complete_with(success, after(2ms))
            .reply_with(connack, after(4ms));

    auto handler = [&](error_code ec) {
        BOOST_TEST(ec == success);
    };

    detail::mqtt_ctx mqtt_ctx;
    mqtt_ctx.co_props = init_connect_props();
    mqtt_ctx.authenticator = test::test_authenticator();
    run_unit_test(std::move(mqtt_ctx), std::move(broker_side), std::move(handler));
}

BOOST_FIXTURE_TEST_CASE(successful_auth_multi_step, shared_test_auth_data) {
    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(2ms))
            .reply_with(auth_challenge, after(3ms))
        .expect(auth_response)
            .complete_with(success, after(2ms))
            .reply_with(auth_challenge, after(4ms))
        .expect(auth_response)
            .complete_with(success, after(2ms))
            .reply_with(connack, after(4ms));

    auto handler = [&](error_code ec) {
        BOOST_TEST(ec == success);
    };

    detail::mqtt_ctx mqtt_ctx;
    mqtt_ctx.co_props = init_connect_props();
    mqtt_ctx.authenticator = test::test_authenticator();
    run_unit_test(std::move(mqtt_ctx), std::move(broker_side), std::move(handler));
}

BOOST_FIXTURE_TEST_CASE(malformed_auth_rc, shared_test_auth_data) {
    auto malformed_auth_challenge = encoders::encode_auth(
        reason_codes::server_busy.value(), init_auth_props()
    );

    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(2ms))
            .reply_with(malformed_auth_challenge, after(3ms));

    auto handler = [&](error_code ec) {
        BOOST_TEST(ec == client::error::malformed_packet);
    };

    detail::mqtt_ctx mqtt_ctx;
    mqtt_ctx.co_props = init_connect_props();
    mqtt_ctx.authenticator = test::test_authenticator();
    run_unit_test(std::move(mqtt_ctx), std::move(broker_side), std::move(handler));
}

BOOST_FIXTURE_TEST_CASE(mismatched_auth_method, shared_test_auth_data) {
    auth_props aprops;
    aprops[prop::authentication_method] = "wrong method";

    auto mismatched_auth_challenge = encoders::encode_auth(
        reason_codes::continue_authentication.value(), aprops
    );

    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(2ms))
            .reply_with(mismatched_auth_challenge, after(3ms));

    auto handler = [&](error_code ec) {
        BOOST_TEST(ec == client::error::malformed_packet);
    };

    detail::mqtt_ctx mqtt_ctx;
    mqtt_ctx.co_props = init_connect_props();
    mqtt_ctx.authenticator = test::test_authenticator();
    run_unit_test(std::move(mqtt_ctx), std::move(broker_side), std::move(handler));
}

BOOST_FIXTURE_TEST_CASE(fail_to_send_auth, shared_test_auth_data) {
    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(2ms))
            .reply_with(auth_challenge, after(3ms))
        .expect(auth_response)
            .complete_with(fail, after(2ms));

    auto handler = [&](error_code ec) {
        BOOST_TEST(ec == fail);
    };

    detail::mqtt_ctx mqtt_ctx;
    mqtt_ctx.co_props = init_connect_props();
    mqtt_ctx.authenticator = test::test_authenticator();
    run_unit_test(std::move(mqtt_ctx), std::move(broker_side), std::move(handler));
}

BOOST_FIXTURE_TEST_CASE(auth_step_client_initial_fail, shared_test_auth_data) {
    test::msg_exchange broker_side;

    auto handler = [&](error_code ec) {
        BOOST_TEST(ec == asio::error::try_again);
    };

    detail::mqtt_ctx mqtt_ctx;
    mqtt_ctx.co_props = init_connect_props();
    mqtt_ctx.authenticator = test::fail_test_authenticator<auth_step_e::client_initial>();
    run_unit_test(std::move(mqtt_ctx), std::move(broker_side), std::move(handler));
}

BOOST_FIXTURE_TEST_CASE(auth_step_server_challenge_fail, shared_test_auth_data) {
    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(2ms))
            .reply_with(auth_challenge, after(3ms));

    auto handler = [&](error_code ec) {
        BOOST_TEST(ec == asio::error::try_again);
    };

    detail::mqtt_ctx mqtt_ctx;
    mqtt_ctx.co_props = init_connect_props();
    mqtt_ctx.authenticator = test::fail_test_authenticator<auth_step_e::server_challenge>();
    run_unit_test(std::move(mqtt_ctx), std::move(broker_side), std::move(handler));
}

BOOST_FIXTURE_TEST_CASE(auth_step_server_final_fail, shared_test_auth_data) {
    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(2ms))
            .reply_with(auth_challenge, after(3ms))
        .expect(auth_response)
            .complete_with(success, after(2ms))
            .reply_with(connack, after(4ms));

    auto handler = [&](error_code ec) {
        BOOST_TEST(ec == asio::error::try_again);
    };

    detail::mqtt_ctx mqtt_ctx;
    mqtt_ctx.co_props = init_connect_props();
    mqtt_ctx.authenticator = test::fail_test_authenticator<auth_step_e::server_final>();
    run_unit_test(std::move(mqtt_ctx), std::move(broker_side), std::move(handler));
}

BOOST_AUTO_TEST_SUITE_END();
