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
#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <limits>
#include <string>

#include "test_common/message_exchange.hpp"
#include "test_common/test_service.hpp"
#include "test_common/test_stream.hpp"

using namespace boost::mqtt5;

BOOST_AUTO_TEST_SUITE(ping/*, *boost::unit_test::disabled()*/)

struct shared_test_data {
    error_code success {};
    error_code fail = asio::error::not_connected;

    const std::string connack_no_ka = encoders::encode_connack(
        false, reason_codes::success.value(), {}
    );

    const std::string pingreq = encoders::encode_pingreq();
    const std::string pingresp = encoders::encode_pingresp();
};

using test::after;
using namespace std::chrono_literals;

std::string connect_with_keep_alive(uint16_t keep_alive) {
    return encoders::encode_connect(
        "", std::nullopt, std::nullopt, keep_alive, false, {}, std::nullopt
    );
}

std::string connack_with_keep_alive(uint16_t keep_alive) {
    connack_props cprops;
    cprops[prop::server_keep_alive] = keep_alive;

    return encoders::encode_connack(
        false, reason_codes::success.value(), cprops
    );
}

void run_test(
    test::msg_exchange broker_side,
    std::chrono::milliseconds cancel_timeout,
    uint16_t keep_alive = (std::numeric_limits<uint16_t>::max)()
) {
    asio::io_context ioc;
    auto executor = ioc.get_executor();
    auto& broker = asio::make_service<test::test_broker>(
        ioc, executor, std::move(broker_side)
    );

    using client_type = mqtt_client<test::test_stream>;
    client_type c(executor);
    c.brokers("127.0.0.1,127.0.0.1") // to avoid reconnect backoff
        .keep_alive(keep_alive)
        .async_run(asio::detached);

    asio::steady_timer timer(c.get_executor());
    timer.expires_after(cancel_timeout);
    timer.async_wait([&c](error_code) {
        c.cancel();
    });

    ioc.run();
    BOOST_TEST(broker.received_all_expected());
}

BOOST_FIXTURE_TEST_CASE(ping_pong_client_ka, shared_test_data) {
    // data
    uint16_t keep_alive = 1;

    test::msg_exchange broker_side;
    broker_side
        .expect(connect_with_keep_alive(keep_alive))
            .complete_with(success, after(1ms))
            .reply_with(connack_no_ka, after(2ms))
        .expect(pingreq)
            .complete_with(success, after(1ms))
            .reply_with(pingresp, after(2ms));

    run_test(
        std::move(broker_side),
        std::chrono::milliseconds(keep_alive * 1000 + 100),
        keep_alive
    );
}

BOOST_FIXTURE_TEST_CASE(ping_pong_server_ka, shared_test_data) {
    // data
    uint16_t keep_alive = 10;
    uint16_t server_keep_alive = 1;

    test::msg_exchange broker_side;
    broker_side
        .expect(connect_with_keep_alive(keep_alive))
            .complete_with(success, after(1ms))
            .reply_with(connack_with_keep_alive(server_keep_alive), after(2ms))
        .expect(pingreq)
            .complete_with(success, after(1ms))
            .reply_with(pingresp, after(2ms));

    run_test(
        std::move(broker_side),
        std::chrono::milliseconds(server_keep_alive * 1000 + 100),
        keep_alive
    );
}

BOOST_FIXTURE_TEST_CASE(disable_ping, shared_test_data) {
    // data
    uint16_t keep_alive = 0;

    test::msg_exchange broker_side;
    broker_side
        .expect(connect_with_keep_alive(keep_alive))
            .complete_with(success, after(1ms))
            .reply_with(connack_no_ka, after(2ms));

    run_test(
        std::move(broker_side),
        std::chrono::milliseconds(1000),
        keep_alive
    );
}

BOOST_FIXTURE_TEST_CASE(ping_timeout, shared_test_data) {
    // observation in test cases with a real broker:
    // old stream_ptr will receive disconnect with rc: session taken over
    // when the new stream_ptr sends a connect packet

    // data
    uint16_t keep_alive = 1;

    test::msg_exchange broker_side;
    broker_side
        .expect(connect_with_keep_alive(keep_alive))
            .complete_with(success, after(1ms))
            .reply_with(connack_no_ka, after(2ms))
        .expect(pingreq)
            .complete_with(success, after(1ms))
        .expect(connect_with_keep_alive(keep_alive))
            .complete_with(success, after(1ms))
            .reply_with(connack_no_ka, after(2ms))
        .expect(pingreq)
            .complete_with(success, after(1ms))
            .reply_with(pingresp, after(2ms));

    run_test(
        std::move(broker_side),
        std::chrono::milliseconds(2700),
        keep_alive
    );
}

BOOST_FIXTURE_TEST_CASE(keep_alive_change_while_waiting, shared_test_data) {
    // data
    uint16_t keep_alive = 0;
    uint16_t server_keep_alive = 1;

    test::msg_exchange broker_side;
    broker_side
        .expect(connect_with_keep_alive(keep_alive))
            .complete_with(success, after(1ms))
            .reply_with(connack_with_keep_alive(server_keep_alive), after(2ms))
        .expect(pingreq)
            .complete_with(success, after(1ms))
            .reply_with(fail, after(2ms))
        .expect(connect_with_keep_alive(keep_alive))
            .complete_with(success, after(1ms))
            .reply_with(connack_no_ka, after(2ms));

    run_test(
        std::move(broker_side),
        std::chrono::milliseconds(1500), keep_alive
    );
}

BOOST_FIXTURE_TEST_CASE(keep_alive_change_during_writing, shared_test_data) {
    // data
    uint16_t keep_alive = 1;
    uint16_t server_keep_alive = 1;

    test::msg_exchange broker_side;
    broker_side
        .expect(connect_with_keep_alive(keep_alive))
            .complete_with(success, after(1ms))
            .reply_with(connack_with_keep_alive(server_keep_alive), after(1500ms))
        .expect(pingreq)
            .complete_with(success, after(1ms))
            .reply_with(pingresp, after(2ms));

    run_test(
        std::move(broker_side),
        std::chrono::milliseconds(2700), keep_alive
    );
}

BOOST_AUTO_TEST_SUITE_END();
