//
// Copyright (c) 2023-2025 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mqtt5/mqtt_client.hpp>
#include <boost/mqtt5/types.hpp>

#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/test/unit_test.hpp>

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "test_common/message_exchange.hpp"
#include "test_common/packet_util.hpp"
#include "test_common/test_service.hpp"
#include "test_common/test_stream.hpp"

using namespace boost::mqtt5;

namespace boost::mqtt5::test {

enum operation_type {
    subscribe = 1,
    unsubscribe
};

} // end namespace boost::mqtt5::test

BOOST_AUTO_TEST_SUITE(sub_unsub/*, *boost::unit_test::disabled()*/)

struct shared_test_data {
    error_code success {};
    error_code fail = asio::error::not_connected;

    const std::string connect = encoders::encode_connect(
        "", std::nullopt, std::nullopt, 60, false, {}, std::nullopt
    );
    const std::string connack = encoders::encode_connack(
        false, reason_codes::success.value(), {}
    );

    std::vector<subscribe_topic> sub_topics = {
        subscribe_topic { "topic", subscribe_options {} }
    };
    std::vector<std::string> unsub_topics = { "topic" };
    std::vector<uint8_t> reason_codes = { uint8_t(0x00) };

    const std::string subscribe = encoders::encode_subscribe(
        1, sub_topics, subscribe_props {}
    );
    const std::string suback = encoders::encode_suback(1, reason_codes, suback_props {});

    const std::string unsubscribe = encoders::encode_unsubscribe(
        1, unsub_topics, unsubscribe_props {}
    );
    const std::string unsuback = encoders::encode_unsuback(1, reason_codes, unsuback_props {});
};

using test::after;
using namespace std::chrono_literals;

template <test::operation_type op_type>
void run_test(test::msg_exchange broker_side) {
    constexpr int expected_handlers_called = 1;
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

    auto data = shared_test_data();
    if constexpr (op_type == test::operation_type::subscribe)
        c.async_subscribe(
            data.sub_topics, subscribe_props {},
            [&handlers_called, &c](error_code ec, std::vector<reason_code> rcs, suback_props) {
                ++handlers_called;

                BOOST_TEST(!ec);
                BOOST_TEST_REQUIRE(rcs.size() == 1u);
                BOOST_TEST(rcs[0] == reason_codes::granted_qos_0);

                c.cancel();
            }
        );
    else 
        c.async_unsubscribe(
            data.unsub_topics, unsubscribe_props {},
            [&handlers_called, &c](error_code ec, std::vector<reason_code> rcs, unsuback_props) {
                ++handlers_called;

                BOOST_TEST(!ec);
                BOOST_TEST_REQUIRE(rcs.size() == 1u);
                BOOST_TEST(rcs[0] == reason_codes::success);

                c.cancel();
            }
        );

    ioc.run_for(5s);
    BOOST_TEST(handlers_called == expected_handlers_called);
    BOOST_TEST(broker.received_all_expected());
}

// subscribe

BOOST_FIXTURE_TEST_CASE(fail_to_send_subscribe, shared_test_data) {
    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(1ms))
            .reply_with(connack, after(2ms))
        .expect(subscribe)
            .complete_with(fail, after(1ms))
        .expect(connect)
            .complete_with(success, after(1ms))
            .reply_with(connack, after(2ms))
        .expect(subscribe)
            .complete_with(success, after(1ms))
            .reply_with(suback, after(2ms));

    run_test<test::operation_type::subscribe>(std::move(broker_side));
}

BOOST_FIXTURE_TEST_CASE(fail_to_receive_suback, shared_test_data) {
    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(1ms))
            .reply_with(connack, after(2ms))
        .expect(subscribe)
            .complete_with(success, after(1ms))
        .send(fail, after(15ms))
        .expect(connect)
            .complete_with(success, after(1ms))
            .reply_with(connack, after(2ms))
        .expect(subscribe)
            .complete_with(success, after(1ms))
            .reply_with(suback, after(2ms));

    run_test<test::operation_type::subscribe>(std::move(broker_side));
}

BOOST_FIXTURE_TEST_CASE(receive_malformed_suback, shared_test_data) {
    // packets
    const char malformed_bytes[] = {
        -112, 7, 0, 1, 4, 31, 0, 2, 32
    };
    std::string malformed_suback { malformed_bytes, sizeof(malformed_bytes) / sizeof(char) };

    auto disconnect = encoders::encode_disconnect(
        reason_codes::malformed_packet.value(),
        test::dprops_with_reason_string("Malformed SUBACK: cannot decode")
    );

    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(1ms))
            .reply_with(connack, after(2ms))
        .expect(subscribe)
            .complete_with(success, after(1ms))
            .reply_with(malformed_suback, after(2ms))
        .expect(disconnect)
            .complete_with(success, after(1ms))
        .expect(connect)
            .complete_with(success, after(1ms))
            .reply_with(connack, after(2ms))
        .expect(subscribe)
            .complete_with(success, after(1ms))
            .reply_with(suback, after(2ms));

    run_test<test::operation_type::subscribe>(std::move(broker_side));
}

BOOST_FIXTURE_TEST_CASE(receive_invalid_rc_in_suback, shared_test_data) {
    // packets
    auto malformed_suback = encoders::encode_suback(
        1, { uint8_t(0x04) }, suback_props {}
    );

    auto disconnect = encoders::encode_disconnect(
        reason_codes::malformed_packet.value(),
        test::dprops_with_reason_string(
            "Malformed SUBACK: does not contain a "
            "valid Reason Code for every Topic Filter"
        )
    );

    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(1ms))
            .reply_with(connack, after(2ms))
        .expect(subscribe)
            .complete_with(success, after(1ms))
            .reply_with(malformed_suback, after(2ms))
        .expect(disconnect)
            .complete_with(success, after(1ms))
        .expect(connect)
            .complete_with(success, after(1ms))
            .reply_with(connack, after(2ms))
        .expect(subscribe)
            .complete_with(success, after(1ms))
            .reply_with(suback, after(2ms));

    run_test<test::operation_type::subscribe>(std::move(broker_side));
}

BOOST_FIXTURE_TEST_CASE(mismatched_num_of_suback_rcs, shared_test_data) {
    // packets
    auto malformed_suback = encoders::encode_suback(
        1, { uint8_t(0x00), uint8_t(0x00) }, suback_props {}
    );

    auto disconnect = encoders::encode_disconnect(
        reason_codes::malformed_packet.value(),
        test::dprops_with_reason_string(
            "Malformed SUBACK: does not contain a "
            "valid Reason Code for every Topic Filter"
        )
    );

    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(1ms))
            .reply_with(connack, after(2ms))
        .expect(subscribe)
            .complete_with(success, after(1ms))
            .reply_with(malformed_suback, after(2ms))
        .expect(disconnect)
            .complete_with(success, after(1ms))
        .expect(connect)
            .complete_with(success, after(1ms))
            .reply_with(connack, after(2ms))
        .expect(subscribe)
            .complete_with(success, after(1ms))
            .reply_with(suback, after(2ms));

    run_test<test::operation_type::subscribe>(std::move(broker_side));
}

// unsubscribe

BOOST_FIXTURE_TEST_CASE(fail_to_send_unsubscribe, shared_test_data) {
    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(1ms))
            .reply_with(connack, after(2ms))
        .expect(unsubscribe)
            .complete_with(fail, after(1ms))
        .expect(connect)
            .complete_with(success, after(1ms))
            .reply_with(connack, after(2ms))
        .expect(unsubscribe)
            .complete_with(success, after(1ms))
            .reply_with(unsuback, after(2ms));

    run_test<test::operation_type::unsubscribe>(std::move(broker_side));
}

BOOST_FIXTURE_TEST_CASE(fail_to_receive_unsuback, shared_test_data) {
    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(1ms))
            .reply_with(connack, after(2ms))
        .expect(unsubscribe)
            .complete_with(success, after(1ms))
        .send(fail, after(15ms))
        .expect(connect)
            .complete_with(success, after(1ms))
            .reply_with(connack, after(2ms))
        .expect(unsubscribe)
            .complete_with(success, after(1ms))
            .reply_with(unsuback, after(2ms));

    run_test<test::operation_type::unsubscribe>(std::move(broker_side));
}

BOOST_FIXTURE_TEST_CASE(receive_malformed_unsuback, shared_test_data) {
    // packets
    const char malformed_bytes[] = {
        -80, 7, 0, 1, 4, 31, 0, 2, 32
    };
    std::string malformed_unsuback { malformed_bytes, sizeof(malformed_bytes) / sizeof(char) };

    auto disconnect = encoders::encode_disconnect(
        reason_codes::malformed_packet.value(),
        test::dprops_with_reason_string("Malformed UNSUBACK: cannot decode")
    );

    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(1ms))
            .reply_with(connack, after(2ms))
        .expect(unsubscribe)
            .complete_with(success, after(1ms))
            .reply_with(malformed_unsuback, after(2ms))
        .expect(disconnect)
            .complete_with(success, after(1ms))
        .expect(connect)
            .complete_with(success, after(1ms))
            .reply_with(connack, after(2ms))
        .expect(unsubscribe)
            .complete_with(success, after(1ms))
            .reply_with(unsuback, after(2ms));

    run_test<test::operation_type::unsubscribe>(std::move(broker_side));
}

BOOST_FIXTURE_TEST_CASE(receive_invalid_rc_in_unsuback, shared_test_data) {
    // packets
    auto malformed_unsuback = encoders::encode_unsuback(
        1, { uint8_t(0x04) }, unsuback_props {}
    );

    auto disconnect = encoders::encode_disconnect(
        reason_codes::malformed_packet.value(),
        test::dprops_with_reason_string(
            "Malformed UNSUBACK: does not contain a "
            "valid Reason Code for every Topic Filter"
        )
    );

    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(1ms))
            .reply_with(connack, after(2ms))
        .expect(unsubscribe)
            .complete_with(success, after(1ms))
            .reply_with(malformed_unsuback, after(2ms))
        .expect(disconnect)
            .complete_with(success, after(1ms))
        .expect(connect)
            .complete_with(success, after(1ms))
            .reply_with(connack, after(2ms))
        .expect(unsubscribe)
            .complete_with(success, after(1ms))
            .reply_with(unsuback, after(2ms));

    run_test<test::operation_type::unsubscribe>(std::move(broker_side));
}

BOOST_FIXTURE_TEST_CASE(mismatched_num_of_unsuback_rcs, shared_test_data) {
    // packets
    auto malformed_unsuback = encoders::encode_unsuback(
        1, { uint8_t(0x00), uint8_t(0x00)}, unsuback_props {}
    );

    auto disconnect = encoders::encode_disconnect(
        reason_codes::malformed_packet.value(),
        test::dprops_with_reason_string(
            "Malformed UNSUBACK: does not contain a "
            "valid Reason Code for every Topic Filter"
        )
    );

    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(1ms))
            .reply_with(connack, after(2ms))
        .expect(unsubscribe)
            .complete_with(success, after(1ms))
            .reply_with(malformed_unsuback, after(2ms))
        .expect(disconnect)
            .complete_with(success, after(1ms))
        .expect(connect)
            .complete_with(success, after(1ms))
            .reply_with(connack, after(2ms))
        .expect(unsubscribe)
            .complete_with(success, after(1ms))
            .reply_with(unsuback, after(2ms));

    run_test<test::operation_type::unsubscribe>(std::move(broker_side));
}

template <test::operation_type op_type>
void run_cancellation_test(test::msg_exchange broker_side) {
    constexpr int expected_handlers_called = 1;
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

    asio::cancellation_signal cancel_signal;
    auto data = shared_test_data();
    if constexpr (op_type == test::operation_type::subscribe)
        c.async_subscribe(
            data.sub_topics, subscribe_props {},
            asio::bind_cancellation_slot(
                cancel_signal.slot(),
                [&handlers_called, &c](error_code ec, std::vector<reason_code> rcs, suback_props) {
                    ++handlers_called;

                    BOOST_TEST(ec == asio::error::operation_aborted);
                    BOOST_TEST_REQUIRE(rcs.size() == 1u);
                    BOOST_TEST(rcs[0] == reason_codes::empty);

                    c.cancel();
                }
            )
        );
    else
        c.async_unsubscribe(
            data.unsub_topics, unsubscribe_props {},
            asio::bind_cancellation_slot(
                cancel_signal.slot(),
                [&handlers_called, &c](error_code ec, std::vector<reason_code> rcs, unsuback_props) {
                    ++handlers_called;

                    BOOST_TEST(ec == asio::error::operation_aborted);
                    BOOST_TEST_REQUIRE(rcs.size() == 1u);
                    BOOST_TEST(rcs[0] == reason_codes::empty);

                    c.cancel();
                }
            )
        );

    cancel_signal.emit(asio::cancellation_type::total);

    ioc.run_for(2s);
    BOOST_TEST(handlers_called == expected_handlers_called);
    BOOST_TEST(broker.received_all_expected());
}

BOOST_FIXTURE_TEST_CASE(cancel_resending_subscribe, shared_test_data) {
    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(1ms))
            .reply_with(connack, after(2ms));

    run_cancellation_test<test::operation_type::subscribe>(std::move(broker_side));
}

BOOST_FIXTURE_TEST_CASE(cancel_resending_unsubscribe, shared_test_data) {
    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(1ms))
            .reply_with(connack, after(2ms));

    run_cancellation_test<test::operation_type::unsubscribe>(std::move(broker_side));
}

BOOST_AUTO_TEST_SUITE_END();
