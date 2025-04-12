//
// Copyright (c) 2023-2025 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mqtt5/mqtt_client.hpp>
#include <boost/mqtt5/types.hpp>

#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/test/unit_test.hpp>

#include <chrono>
#include <cstdint>
#include <string>

#include "test_common/message_exchange.hpp"
#include "test_common/test_service.hpp"
#include "test_common/test_stream.hpp"

using namespace boost::mqtt5;

BOOST_AUTO_TEST_SUITE(read_message/*, *boost::unit_test::disabled()*/)

using test::after;
using namespace std::chrono_literals;

void test_receive_malformed_packet(
    std::string malformed_packet, std::string reason_string
) {
    // packets
    auto connect = encoders::encode_connect(
        "", std::nullopt, std::nullopt, 60, false, {}, std::nullopt
    );
    connack_props co_props;
    co_props[prop::maximum_packet_size] = 2000;
    auto connack = encoders::encode_connack(false, reason_codes::success.value(), co_props);

    disconnect_props dc_props;
    dc_props[prop::reason_string] = reason_string;
    auto disconnect = encoders::encode_disconnect(
        reason_codes::malformed_packet.value(), dc_props
    );

    error_code success {};
    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(0ms))
            .reply_with(connack, after(0ms))
        .send(malformed_packet, after(5ms))
        .expect(disconnect)
            .complete_with(success, after(0ms))
        .expect(connect)
            .complete_with(success, after(0ms))
            .reply_with(connack, after(0ms));


    asio::io_context ioc;
    auto executor = ioc.get_executor();
    auto& broker = asio::make_service<test::test_broker>(
        ioc, executor, std::move(broker_side)
    );

    using client_type = mqtt_client<test::test_stream>;
    client_type c(executor);
    c.brokers("127.0.0.1,127.0.0.1") // to avoid reconnect backoff
        .async_run(asio::detached);

    asio::steady_timer timer(c.get_executor());
    timer.expires_after(100ms);
    timer.async_wait([&c](error_code) { c.cancel(); });

    ioc.run();
    BOOST_TEST(broker.received_all_expected());
}

BOOST_AUTO_TEST_CASE(forbidden_packet_type) {
    test_receive_malformed_packet(
        std::string({ 0x00 }),
        "Malformed Packet received from the Server"
    );
}

BOOST_AUTO_TEST_CASE(malformed_varint) {
    test_receive_malformed_packet(
        std::string({ 0x10, -1 /* 0xFF */, -1, -1, -1 }),
        "Malformed Packet received from the Server"
    );
}

BOOST_AUTO_TEST_CASE(malformed_fixed_header) {
    test_receive_malformed_packet(
        std::string({ 0x60, 1, 0 }),
        "Malformed Packet received from the Server"
    );
}

BOOST_AUTO_TEST_CASE(packet_larger_than_allowed) {
    test_receive_malformed_packet(
        std::string({ 0x10, -1, -1, -1, 0 }),
        "Malformed Packet received from the Server"
    );
}

BOOST_AUTO_TEST_CASE(receive_malformed_publish) {
    test_receive_malformed_packet(
        std::string({ 0x30, 1, -1 }),
        "Malformed PUBLISH received: cannot decode"
    );
}

struct shared_test_data {
    error_code success{};
    error_code fail = asio::error::not_connected;

    const std::string topic = "topic";
    const std::string payload = "payload";

    const std::string connect = encoders::encode_connect(
        "", std::nullopt, std::nullopt, 60, false, {}, std::nullopt
    );
    const std::string connack = encoders::encode_connack(false, uint8_t(0x00), {});

    const std::string publish = encoders::encode_publish(
        0, topic, payload, qos_e::at_most_once, retain_e::no, dup_e::no, {}
    );

    std::string encode_disconnect_with_rs(std::string_view reason_string) {
        disconnect_props dc_props;
        dc_props[prop::reason_string] = reason_string;
        return encoders::encode_disconnect(
            reason_codes::malformed_packet.value(), dc_props
        );
    }
};

BOOST_FIXTURE_TEST_CASE(receive_disconnect, shared_test_data) {
    // packets
    auto disconnect = encoders::encode_disconnect(0x00, {});

    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(0ms))
            .reply_with(connack, after(0ms))
        .send(disconnect, after(50ms))
        .expect(connect)
            .complete_with(success, after(0ms))
            .reply_with(connack, after(0ms));

    asio::io_context ioc;
    auto executor = ioc.get_executor();
    auto& broker = asio::make_service<test::test_broker>(
        ioc, executor, std::move(broker_side)
    );

    using client_type = mqtt_client<test::test_stream>;
    client_type c(executor);
    c.brokers("127.0.0.1,127.0.0.1") // to avoid reconnect backoff
        .async_run(asio::detached);

    asio::steady_timer timer(c.get_executor());
    timer.expires_after(100ms);
    timer.async_wait([&c](error_code) { c.cancel(); });

    ioc.run();
    BOOST_TEST(broker.received_all_expected());
}

BOOST_FIXTURE_TEST_CASE(receive_disconnect_while_reconnecting, shared_test_data) {
    // packets
    auto disconnect = encoders::encode_disconnect(0x00, {});
    constexpr int expected_handlers_called = 1;
    int handlers_called = 0;

    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(1ms))
            .reply_with(connack, after(2ms))
        .expect(publish)
            .complete_with(fail, after(20ms))
        .send(disconnect, after(30ms))
        .expect(connect)
            .complete_with(success, after(20ms))
            .reply_with(connack, after(30ms))
        .expect(publish)
            .complete_with(success, after(0ms));

    asio::io_context ioc;
    auto executor = ioc.get_executor();
    auto& broker = asio::make_service<test::test_broker>(
        ioc, executor, std::move(broker_side)
    );

    using client_type = mqtt_client<test::test_stream>;
    client_type c(executor);
    c.brokers("127.0.0.1,127.0.0.1") // to avoid reconnect backoff
        .async_run(asio::detached);

    c.async_publish<qos_e::at_most_once>(
        topic, payload, retain_e::no, {},
        [&](error_code ec) {
            BOOST_TEST(handlers_called == 0);
            handlers_called++;
            BOOST_TEST(!ec);
            c.cancel();
        }
    );

    ioc.run_for(1s);
    BOOST_TEST(handlers_called == expected_handlers_called);
    BOOST_TEST(broker.received_all_expected());
}

template <typename VerifyFun>
void run_receive_test(
    test::msg_exchange broker_side, int num_of_receives,
    VerifyFun&& verify_fun
) {
    const int expected_handlers_called = num_of_receives;
    int handlers_called = 0;

    asio::io_context ioc;
    auto executor = ioc.get_executor();
    auto& broker = asio::make_service<test::test_broker>(
        ioc, executor, std::move(broker_side)
    );

    using client_type = mqtt_client<test::test_stream>;
    client_type c(executor);
    c.brokers("127.0.0.1,127.0.0.1") // to avoid reconnect backoff
        .async_run(asio::detached);

    for (int i = 0; i < num_of_receives; ++i)
        c.async_receive([&](
            error_code ec,
            std::string topic, std::string payload, publish_props props
        ) {
            handlers_called++;
            verify_fun(ec, topic, payload, props);

            if (handlers_called == expected_handlers_called)
                c.cancel();
        });

    ioc.run();
    BOOST_TEST(handlers_called == expected_handlers_called);
    BOOST_TEST(broker.received_all_expected());
}

BOOST_FIXTURE_TEST_CASE(receive_byte_by_byte, shared_test_data) {
    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(0ms))
            .reply_with(connack, after(0ms));

    for (size_t i = 0; i < publish.size(); i++)
        broker_side.send(
            std::string { publish[i] }, after(std::chrono::milliseconds(i + 7))
        );

    auto verify_fun = [&](
        error_code ec, std::string topic_, std::string payload_, publish_props
    ) {
        BOOST_TEST(!ec);
        BOOST_TEST(topic == topic_);
        BOOST_TEST(payload == payload_);
    };

    run_receive_test(std::move(broker_side), 1, std::move(verify_fun));
}

BOOST_FIXTURE_TEST_CASE(receive_multiple_packets_at_once, shared_test_data) {
    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(1ms))
            .reply_with(connack, after(2ms))
        .send(publish, publish, publish, publish, publish, after(100ms));

    auto verify_fun = [&](
        error_code ec, std::string topic_, std::string payload_, publish_props
        ) {
            BOOST_TEST(!ec);
            BOOST_TEST(topic == topic_);
            BOOST_TEST(payload == payload_);
        };

    run_receive_test(std::move(broker_side), 5, std::move(verify_fun));
}

BOOST_FIXTURE_TEST_CASE(receive_multiple_packets_with_malformed, shared_test_data) {
    // packets
    // ghost publishes need to be cleared once the client reads the malformed packet
    auto ghost_publish = encoders::encode_publish(
        0, topic, "should not be received!", qos_e::at_most_once, retain_e::no, dup_e::no, {}
    );

    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(1ms))
            .reply_with(connack, after(2ms))
        .send(
            publish, publish, std::string({ 0x00 }) /* malformed */,
            ghost_publish, ghost_publish, after(100ms)
        )
        .expect(encode_disconnect_with_rs("Malformed Packet received from the Server"))
        .expect(connect)
            .complete_with(success, after(1ms))
            .reply_with(connack, after(2ms))
        .send(publish, after(300ms));

    auto verify_fun = [&](
        error_code ec, std::string topic_, std::string payload_, publish_props
    ) {
        BOOST_TEST(!ec);
        BOOST_TEST(topic == topic_);
        BOOST_TEST(payload == payload_);
    };

    run_receive_test(std::move(broker_side), 3, std::move(verify_fun));
}

BOOST_AUTO_TEST_SUITE_END();
