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
#include <optional>
#include <string>
#include <vector>

#include "test_common/message_exchange.hpp"
#include "test_common/test_service.hpp"
#include "test_common/test_stream.hpp"

using namespace boost::mqtt5;

BOOST_AUTO_TEST_SUITE(async_sender/*, *boost::unit_test::disabled()*/)

struct shared_test_data {
    error_code success {};
    error_code fail = asio::error::not_connected;

    const std::string topic = "topic";
    const std::string payload = "payload";

    const std::string connect = encoders::encode_connect(
        "", std::nullopt, std::nullopt, 60, false, {}, std::nullopt
    );
    const std::string connack = encoders::encode_connack(
        false, reason_codes::success.value(), {}
    );
    std::string connack_rm;

    const std::string publish_qos1 = encoders::encode_publish(
        1, topic, payload, qos_e::at_least_once, retain_e::no, dup_e::no, {}
    );
    const std::string publish_qos1_dup = encoders::encode_publish(
        1, topic, payload, qos_e::at_least_once, retain_e::no, dup_e::yes, {}
    );

    const std::string puback = encoders::encode_puback(1, uint8_t(0x00), {});

    shared_test_data() {
        connack_props props;
        props[prop::receive_maximum] = uint16_t(1);

        connack_rm = encoders::encode_connack(
            false, reason_codes::success.value(), props
        );
    }
};

using test::after;
using namespace std::chrono_literals;

BOOST_FIXTURE_TEST_CASE(publish_ordering_after_reconnect, shared_test_data) {
    constexpr int expected_handlers_called = 2;
    int handlers_called = 0;

    // packets
    auto publish_qos2 = encoders::encode_publish(
        2, topic, payload, qos_e::exactly_once, retain_e::no, dup_e::no, {}
    );

    auto pubrec = encoders::encode_pubrec(2, uint8_t(0x00), {});
    auto pubrel = encoders::encode_pubrel(2, uint8_t(0x00), {});
    auto pubcomp = encoders::encode_pubcomp(2, uint8_t(0x00), {});

    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(1ms))
            .reply_with(connack, after(2ms))
        .expect(publish_qos1, publish_qos2)
            .complete_with(success, after(1ms))
            .reply_with(pubrec, after(2ms))
        .expect(pubrel)
            .complete_with(fail, after(1ms))
        .expect(connect)
            .complete_with(success, after(1ms))
            .reply_with(connack, after(2ms))
        .expect(pubrel, publish_qos1_dup)
            .complete_with(success, after(1ms))
            .reply_with(pubcomp, puback, after(2ms));

    asio::io_context ioc;
    auto executor = ioc.get_executor();
    auto& broker = asio::make_service<test::test_broker>(
        ioc, executor, std::move(broker_side)
    );

    using client_type = mqtt_client<test::test_stream>;
    client_type c(executor);
    c.brokers("127.0.0.1,127.0.0.1") // to avoid reconnect backoff
        .async_run(asio::detached);

    c.async_publish<qos_e::at_least_once>(
        topic, payload, retain_e::no, publish_props {},
        [&](error_code ec, reason_code rc, puback_props) {
            ++handlers_called;

            BOOST_TEST(!ec);
            BOOST_TEST(rc == reason_codes::success);

            if (handlers_called == expected_handlers_called)
                c.cancel();
        }
    );

    c.async_publish<qos_e::exactly_once>(
        topic, payload, retain_e::no, publish_props{},
        [&](error_code ec, reason_code rc, pubcomp_props) {
            ++handlers_called;

            BOOST_TEST(!ec);
            BOOST_TEST(rc == reason_codes::success);

            if (handlers_called == expected_handlers_called)
                c.cancel();
        }
    );

    ioc.run_for(1s);
    BOOST_TEST(handlers_called == expected_handlers_called);
    BOOST_TEST(broker.received_all_expected());
}

BOOST_FIXTURE_TEST_CASE(sub_unsub_ordering_after_reconnect, shared_test_data) {
    constexpr int expected_handlers_called = 2;
    int handlers_called = 0;

    // data
    std::vector<subscribe_topic> sub_topics = {
        subscribe_topic { "topic", subscribe_options {} }
    };
    std::vector<uint8_t> sub_reason_codes = {
        reason_codes::granted_qos_2.value()
    };
    std::vector<std::string> unsub_topics = { "topic" };
    std::vector<uint8_t> unsub_reason_codes = { reason_codes::success.value() };

    // packets
    auto subscribe = encoders::encode_subscribe(
        1, sub_topics, subscribe_props {}
    );
    auto suback = encoders::encode_suback(1, sub_reason_codes, suback_props {});
    auto unsubscribe = encoders::encode_unsubscribe(
        2, unsub_topics, unsubscribe_props {}
    );
    auto unsuback = encoders::encode_unsuback(2, unsub_reason_codes, unsuback_props {});
    auto disconnect = encoders::encode_disconnect(0x00, {});

    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(1ms))
            .reply_with(connack, after(2ms))
        .expect(subscribe, unsubscribe)
            .complete_with(success, after(1ms))
        .send(disconnect, after(50ms))
        .expect(connect)
            .complete_with(success, after(1ms))
            .reply_with(connack, after(2ms))
        .expect(subscribe, unsubscribe)
            .complete_with(success, after(1ms))
            .reply_with(suback, unsuback, after(2ms));

    asio::io_context ioc;
    auto executor = ioc.get_executor();
    auto& broker = asio::make_service<test::test_broker>(
        ioc, executor, std::move(broker_side)
    );

    using client_type = mqtt_client<test::test_stream>;
    client_type c(executor);
    c.brokers("127.0.0.1,127.0.0.1") // to avoid reconnect backoff
        .async_run(asio::detached);

    c.async_subscribe(
        sub_topics, subscribe_props {},
        [&](error_code ec, std::vector<reason_code> rcs, suback_props) {
            ++handlers_called;

            BOOST_TEST(!ec);
            BOOST_TEST_REQUIRE(rcs.size() == 1u);
            BOOST_TEST(rcs[0] == reason_codes::granted_qos_2);
        }
    );
    c.async_unsubscribe(
        unsub_topics, unsubscribe_props {},
        [&](error_code ec, std::vector<reason_code> rcs, unsuback_props) {
            ++handlers_called;

            BOOST_TEST(!ec);
            BOOST_TEST_REQUIRE(rcs.size() == 1u);
            BOOST_TEST(rcs[0] == reason_codes::success);

            c.cancel();
        }
    );

    ioc.run_for(1s);
    BOOST_TEST(handlers_called == expected_handlers_called);
    BOOST_TEST(broker.received_all_expected());
}

BOOST_FIXTURE_TEST_CASE(throttling, shared_test_data) {
    constexpr int expected_handlers_called = 3;
    int handlers_called = 0;

    // packets
    auto publish_1 = encoders::encode_publish(
        1, topic, payload, qos_e::at_least_once, retain_e::no, dup_e::no, {}
    );
    auto publish_2 = encoders::encode_publish(
        2, topic, payload, qos_e::at_least_once, retain_e::no, dup_e::no, {}
    );
    auto publish_3 = encoders::encode_publish(
        3, topic, payload, qos_e::at_least_once, retain_e::no, dup_e::no, {}
    );

    auto puback_1 = encoders::encode_puback(1, uint8_t(0x00), {});
    auto puback_2 = encoders::encode_puback(2, uint8_t(0x00), {});
    auto puback_3 = encoders::encode_puback(3, uint8_t(0x00), {});

    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(1ms))
            .reply_with(connack_rm, after(2ms))
        .expect(publish_1)
            .complete_with(success, after(1ms))
            .reply_with(puback_1, after(2ms))
        .expect(publish_2)
            .complete_with(success, after(1ms))
            .reply_with(puback_2, after(2ms))
        .expect(publish_3)
            .complete_with(success, after(1ms))
            .reply_with(puback_3, after(2ms));

    asio::io_context ioc;
    auto executor = ioc.get_executor();
    auto& broker = asio::make_service<test::test_broker>(
        ioc, executor, std::move(broker_side)
    );

    using client_type = mqtt_client<test::test_stream>;
    client_type c(executor);
    c.brokers("127.0.0.1")
        .async_run(asio::detached);

    for (auto i = 0; i < 3; i++)
        c.async_publish<qos_e::at_least_once>(
            topic, payload, retain_e::no, publish_props {},
            [&c, &handlers_called, i](error_code ec, reason_code rc, puback_props) {
                ++handlers_called;

                BOOST_TEST(!ec);
                BOOST_TEST(rc == reason_codes::success);
                BOOST_TEST(handlers_called == i + 1);

                if (i == 2)
                    c.cancel();
            }
        );

    ioc.run_for(1s);
    BOOST_TEST(handlers_called == expected_handlers_called);
    BOOST_TEST(broker.received_all_expected());
}

BOOST_FIXTURE_TEST_CASE(throttling_ordering, shared_test_data) {
    constexpr int expected_handlers_called = 2;
    int handlers_called = 0;

    // packets
    auto publish_qos0 = encoders::encode_publish(
        0, topic, payload, qos_e::at_most_once, retain_e::no, dup_e::no, {}
    );

    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(1ms))
            .reply_with(connack, after(2ms))
        .expect(publish_qos1, publish_qos0)
            .complete_with(success, after(1ms))
            .reply_with(puback, after(2ms));

    asio::io_context ioc;
    auto executor = ioc.get_executor();
    auto& broker = asio::make_service<test::test_broker>(
        ioc, executor, std::move(broker_side)
    );

    using client_type = mqtt_client<test::test_stream>;
    client_type c(executor);
    c.brokers("127.0.0.1,127.0.0.1") // to avoid reconnect backoff
        .async_run(asio::detached);

    c.async_publish<qos_e::at_least_once>(
        topic, payload, retain_e::no, publish_props {},
        [&](error_code ec, reason_code rc, puback_props) {
            ++handlers_called;

            BOOST_TEST(!ec);
            BOOST_TEST(rc == reason_codes::success);

            if (handlers_called == expected_handlers_called)
                c.cancel();
        }
    );

    c.async_publish<qos_e::at_most_once>(
        topic, payload, retain_e::no, publish_props{},
        [&](error_code ec) {
            ++handlers_called;

            BOOST_TEST(!ec);

            if (handlers_called == expected_handlers_called)
                c.cancel();
        }
    );

    ioc.run_for(1s);
    BOOST_TEST(handlers_called == expected_handlers_called);
    BOOST_TEST(broker.received_all_expected());
}

BOOST_FIXTURE_TEST_CASE(prioritize_disconnect, shared_test_data) {
    constexpr int expected_handlers_called = 3;
    int handlers_called = 0;

    // data
    std::vector<subscribe_topic> sub_topics = {
        subscribe_topic { "topic", subscribe_options {} }
    };
    std::vector<uint8_t> reason_codes = { uint8_t(0x00) };

    // packets
    auto disconnect = encoders::encode_disconnect(uint8_t(0x00), {});
    auto subscribe = encoders::encode_subscribe(
        1, sub_topics, subscribe_props {}
    );
    auto suback = encoders::encode_suback(1, reason_codes, suback_props {});

    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(1ms))
            .reply_with(connack, after(2ms))
        .expect(publish_qos1) // first one goes alone
            .complete_with(success, after(3ms))
        .expect(disconnect) // disconnect overrides a subscribe request
            .complete_with(success, after(1ms));

    asio::io_context ioc;
    auto executor = ioc.get_executor();
    auto& broker = asio::make_service<test::test_broker>(
        ioc, executor, std::move(broker_side)
    );

    using client_type = mqtt_client<test::test_stream>;
    client_type c(executor);
    c.brokers("127.0.0.1,127.0.0.1") // to avoid reconnect backoff
        .async_run(asio::detached);

    // give time to establish a connection
    asio::steady_timer timer(executor);
    timer.expires_after(100ms);
    timer.async_wait([&](error_code) {
        c.async_publish<qos_e::at_least_once>(
            topic, payload, retain_e::no, publish_props {},
            [&](error_code ec, reason_code rc, puback_props) {
                ++handlers_called;

                BOOST_TEST(ec == asio::error::operation_aborted);
                BOOST_TEST(rc == reason_codes::empty);
            }
        );

        c.async_subscribe(
            sub_topics, subscribe_props {},
            [&](error_code ec, std::vector<reason_code> rcs, suback_props) {
                ++handlers_called;

                BOOST_TEST(ec == asio::error::operation_aborted);
                BOOST_TEST_REQUIRE(rcs.size() == 1u);
                BOOST_TEST(rcs[0] == reason_codes::empty);
            }
        );

        c.async_disconnect([&](error_code ec) {
            ++handlers_called;
            BOOST_TEST(!ec);
        });
    });

    ioc.run_for(2s);
    BOOST_TEST(handlers_called == expected_handlers_called);
    BOOST_TEST(broker.received_all_expected());
}

BOOST_AUTO_TEST_SUITE_END();
