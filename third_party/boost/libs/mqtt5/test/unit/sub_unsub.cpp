//
// Copyright (c) 2023-2025 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "test_common/message_exchange.hpp"
#include "test_common/packet_util.hpp"
#include "test_common/test_stream.hpp"

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
        "", std::nullopt, std::nullopt, 60, false, test::dflt_cprops, std::nullopt
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

using client_type = mqtt_client<test::test_stream>;

template <typename TestBody>
void run_test(
    test::msg_exchange broker_side, TestBody&& test_body,
    int expected_handlers_called
) {
    int handlers_called = 0;

    asio::io_context ioc;
    auto executor = ioc.get_executor();
    auto& broker = asio::make_service<test::test_broker>(
        ioc, executor, std::move(broker_side)
    );

    client_type c(executor);
    c.brokers("127.0.0.1,127.0.0.1") // to avoid reconnect backoff
        .async_run(asio::detached);

    test_body(c, handlers_called);

    broker.run(ioc);
    BOOST_TEST(handlers_called == expected_handlers_called);
    BOOST_TEST(broker.received_all_expected());
}

template <test::operation_type op_type>
void run_basic_test(test::msg_exchange broker_side) {
    auto body = [](client_type& c, int& handlers_called) {
        auto handler = [&handlers_called, &c](
            error_code ec, std::vector<reason_code> rcs, auto
        ) {
            ++handlers_called;

            BOOST_TEST(!ec);
            BOOST_TEST_REQUIRE(rcs.size() == 1u);
            if constexpr (op_type == test::operation_type::subscribe)
                BOOST_TEST(rcs[0] == reason_codes::granted_qos_0);
            else
                BOOST_TEST(rcs[0] == reason_codes::success);

            c.cancel();
        };

        shared_test_data data;
        if constexpr (op_type == test::operation_type::subscribe)
            c.async_subscribe(data.sub_topics, subscribe_props {}, handler);
        else
            c.async_unsubscribe(data.unsub_topics, unsubscribe_props {}, handler);
    };
    return run_test(std::move(broker_side), std::move(body), 1);
}

// subscribe

BOOST_FIXTURE_TEST_CASE(fail_to_send_subscribe, shared_test_data) {
    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(0ms))
            .reply_with(connack, after(0ms))
        .expect(subscribe)
            .complete_with(fail, after(0ms))
        .expect(connect)
            .complete_with(success, after(0ms))
            .reply_with(connack, after(0ms))
        .expect(subscribe)
            .complete_with(success, after(0ms))
            .reply_with(suback, after(0ms));

    run_basic_test<test::operation_type::subscribe>(std::move(broker_side));
}

BOOST_FIXTURE_TEST_CASE(fail_to_receive_suback, shared_test_data) {
    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(0ms))
            .reply_with(connack, after(0ms))
        .expect(subscribe)
            .complete_with(success, after(0ms))
        .send(fail, after(30ms))
        .expect(connect)
            .complete_with(success, after(0ms))
            .reply_with(connack, after(0ms))
        .expect(subscribe)
            .complete_with(success, after(0ms))
            .reply_with(suback, after(0ms));

    run_basic_test<test::operation_type::subscribe>(std::move(broker_side));
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
            .complete_with(success, after(0ms))
            .reply_with(connack, after(0ms))
        .expect(subscribe)
            .complete_with(success, after(0ms))
            .reply_with(malformed_suback, after(0ms))
        .expect(disconnect)
            .complete_with(success, after(0ms))
        .expect(connect)
            .complete_with(success, after(0ms))
            .reply_with(connack, after(0ms))
        .expect(subscribe)
            .complete_with(success, after(0ms))
            .reply_with(suback, after(0ms));

    run_basic_test<test::operation_type::subscribe>(std::move(broker_side));
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
            .complete_with(success, after(0ms))
            .reply_with(connack, after(0ms))
        .expect(subscribe)
            .complete_with(success, after(0ms))
            .reply_with(malformed_suback, after(0ms))
        .expect(disconnect)
            .complete_with(success, after(0ms))
        .expect(connect)
            .complete_with(success, after(0ms))
            .reply_with(connack, after(0ms))
        .expect(subscribe)
            .complete_with(success, after(0ms))
            .reply_with(suback, after(0ms));

    run_basic_test<test::operation_type::subscribe>(std::move(broker_side));
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
            .complete_with(success, after(0ms))
            .reply_with(connack, after(0ms))
        .expect(subscribe)
            .complete_with(success, after(0ms))
            .reply_with(malformed_suback, after(0ms))
        .expect(disconnect)
            .complete_with(success, after(0ms))
        .expect(connect)
            .complete_with(success, after(0ms))
            .reply_with(connack, after(0ms))
        .expect(subscribe)
            .complete_with(success, after(0ms))
            .reply_with(suback, after(0ms));

    run_basic_test<test::operation_type::subscribe>(std::move(broker_side));
}

// unsubscribe

BOOST_FIXTURE_TEST_CASE(fail_to_send_unsubscribe, shared_test_data) {
    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(0ms))
            .reply_with(connack, after(0ms))
        .expect(unsubscribe)
            .complete_with(fail, after(0ms))
        .expect(connect)
            .complete_with(success, after(0ms))
            .reply_with(connack, after(0ms))
        .expect(unsubscribe)
            .complete_with(success, after(0ms))
            .reply_with(unsuback, after(0ms));

    run_basic_test<test::operation_type::unsubscribe>(std::move(broker_side));
}

BOOST_FIXTURE_TEST_CASE(fail_to_receive_unsuback, shared_test_data) {
    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(0ms))
            .reply_with(connack, after(0ms))
        .expect(unsubscribe)
            .complete_with(success, after(0ms))
        .send(fail, after(30ms))
        .expect(connect)
            .complete_with(success, after(0ms))
            .reply_with(connack, after(0ms))
        .expect(unsubscribe)
            .complete_with(success, after(0ms))
            .reply_with(unsuback, after(0ms));

    run_basic_test<test::operation_type::unsubscribe>(std::move(broker_side));
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
            .complete_with(success, after(0ms))
            .reply_with(connack, after(0ms))
        .expect(unsubscribe)
            .complete_with(success, after(0ms))
            .reply_with(malformed_unsuback, after(0ms))
        .expect(disconnect)
            .complete_with(success, after(0ms))
        .expect(connect)
            .complete_with(success, after(0ms))
            .reply_with(connack, after(0ms))
        .expect(unsubscribe)
            .complete_with(success, after(0ms))
            .reply_with(unsuback, after(0ms));

    run_basic_test<test::operation_type::unsubscribe>(std::move(broker_side));
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
            .complete_with(success, after(0ms))
            .reply_with(connack, after(0ms))
        .expect(unsubscribe)
            .complete_with(success, after(0ms))
            .reply_with(malformed_unsuback, after(0ms))
        .expect(disconnect)
            .complete_with(success, after(0ms))
        .expect(connect)
            .complete_with(success, after(0ms))
            .reply_with(connack, after(0ms))
        .expect(unsubscribe)
            .complete_with(success, after(0ms))
            .reply_with(unsuback, after(0ms));

    run_basic_test<test::operation_type::unsubscribe>(std::move(broker_side));
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
            .complete_with(success, after(0ms))
            .reply_with(connack, after(0ms))
        .expect(unsubscribe)
            .complete_with(success, after(0ms))
            .reply_with(malformed_unsuback, after(0ms))
        .expect(disconnect)
            .complete_with(success, after(0ms))
        .expect(connect)
            .complete_with(success, after(0ms))
            .reply_with(connack, after(0ms))
        .expect(unsubscribe)
            .complete_with(success, after(0ms))
            .reply_with(unsuback, after(0ms));

    run_basic_test<test::operation_type::unsubscribe>(std::move(broker_side));
}

template <test::operation_type op_type>
void run_cancellation_test(test::msg_exchange broker_side) {
    asio::cancellation_signal cancel_signal;

    auto body = [&cancel_signal](client_type& c, int& handlers_called) {
        auto handler = [&handlers_called, &c](
            error_code ec, std::vector<reason_code> rcs, auto
        ) {
            ++handlers_called;

            BOOST_TEST(ec == asio::error::operation_aborted);
            BOOST_TEST_REQUIRE(rcs.size() == 1u);
            BOOST_TEST(rcs[0] == reason_codes::empty);

            c.cancel();
        };
        auto bound_handler = asio::bind_cancellation_slot(
            cancel_signal.slot(), handler
        );

        auto data = shared_test_data();
        if constexpr (op_type == test::operation_type::subscribe)
            c.async_subscribe(data.sub_topics, subscribe_props{}, bound_handler);
        else
            c.async_unsubscribe(data.unsub_topics, unsubscribe_props{}, bound_handler);

        cancel_signal.emit(asio::cancellation_type::total);
    };

    run_test(std::move(broker_side), std::move(body), 1);
}

BOOST_FIXTURE_TEST_CASE(cancel_resending_subscribe, shared_test_data) {
    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(0ms))
            .reply_with(connack, after(0ms));

    run_cancellation_test<test::operation_type::subscribe>(std::move(broker_side));
}

BOOST_FIXTURE_TEST_CASE(cancel_resending_unsubscribe, shared_test_data) {
    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(0ms))
            .reply_with(connack, after(0ms));

    run_cancellation_test<test::operation_type::unsubscribe>(std::move(broker_side));
}

template <prop::property_type p>
void run_sub_validation_test(
    std::integral_constant<prop::property_type, p> prop,
    prop::value_type_t<p> val, error_code expected_ec
) {
    auto shared_data = shared_test_data();

    connack_props props;
    props[prop] = val;
    auto connack = encoders::encode_connack(
        false, reason_codes::success.value(), props
    );

    test::msg_exchange broker_side;
    broker_side
        .expect(shared_data.connect)
            .complete_with(shared_data.success, after(0ms))
            .reply_with(connack, after(0ms));

    auto body = [expected_ec](client_type& c, int& handlers_called) {
        subscribe_props sprops;
        sprops[prop::subscription_identifier] = 42;

        c.async_subscribe(
            subscribe_topic { "$share/test/#", {} }, sprops,
            [&handlers_called, &c, expected_ec]
            (error_code ec, std::vector<reason_code> rcs, suback_props) {
                ++handlers_called;

                BOOST_TEST(ec == expected_ec);
                BOOST_TEST_REQUIRE(rcs.size() == 1u);
                BOOST_TEST(rcs[0] == reason_codes::empty);

                c.cancel();
            }
        );
    };

    run_test(std::move(broker_side), std::move(body), 1);
}

BOOST_FIXTURE_TEST_CASE(packet_too_large, shared_test_data) {
    run_sub_validation_test(
        prop::maximum_packet_size, 10, client::error::packet_too_large
    );
}

BOOST_FIXTURE_TEST_CASE(subid_not_available, shared_test_data) {
    run_sub_validation_test(
        prop::subscription_identifier_available, 0,
        client::error::subscription_identifier_not_available
    );
}

BOOST_FIXTURE_TEST_CASE(shared_not_available, shared_test_data) {
    run_sub_validation_test(
        prop::shared_subscription_available, 0,
        client::error::shared_subscription_not_available
    );
}

BOOST_FIXTURE_TEST_CASE(wildcard_not_available, shared_test_data) {
    run_sub_validation_test(
        prop::wildcard_subscription_available, 0,
        client::error::wildcard_subscription_not_available
    );
}

BOOST_FIXTURE_TEST_CASE(unsubscribe_too_large, shared_test_data) {
    connack_props props;
    props[prop::maximum_packet_size] = 10;
    auto connack = encoders::encode_connack(
        false, reason_codes::success.value(), props
    );

    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(0ms))
            .reply_with(connack, after(0ms));

    auto body = [this](client_type& c, int& handlers_called) {
        c.async_unsubscribe(
            unsub_topics, unsubscribe_props{},
            [&handlers_called, &c]
            (error_code ec, std::vector<reason_code> rcs, unsuback_props) {
                ++handlers_called;

                BOOST_TEST(ec == client::error::packet_too_large);
                BOOST_TEST_REQUIRE(rcs.size() == 1u);
                BOOST_TEST(rcs[0] == reason_codes::empty);

                c.cancel();
            }
        );
    };

    run_test(std::move(broker_side), std::move(body), 1);
}

BOOST_AUTO_TEST_SUITE_END();
