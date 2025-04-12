//
// Copyright (c) 2023-2025 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mqtt5/types.hpp>

#include <boost/mqtt5/impl/client_service.hpp>
#include <boost/mqtt5/impl/publish_send_op.hpp>

#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/test/unit_test.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "test_common/test_service.hpp"

using namespace boost::mqtt5;

BOOST_AUTO_TEST_SUITE(publish_send_op/*, *boost::unit_test::disabled()*/)

BOOST_AUTO_TEST_CASE(pid_overrun) {
    constexpr int expected_handlers_called = 1;
    int handlers_called = 0;

    asio::io_context ioc;
    using client_service_type = test::overrun_client<asio::ip::tcp::socket>;
    auto svc_ptr = std::make_shared<client_service_type>(ioc.get_executor());

    auto handler = [&handlers_called](error_code ec, reason_code rc, puback_props) {
        ++handlers_called;
        BOOST_TEST(ec == client::error::pid_overrun);
        BOOST_TEST(rc == reason_codes::empty);
    };

    detail::publish_send_op<
        client_service_type, decltype(handler), qos_e::at_least_once
    > { svc_ptr, std::move(handler) }
        .perform(
            "test", "payload", retain_e::no, {}
        );

    ioc.run_for(std::chrono::milliseconds(500));
    BOOST_TEST(handlers_called == expected_handlers_called);
}

void run_test(
    error_code expected_ec,
    const std::string& topic_name, const std::string& payload = {},
    const publish_props& pprops = {}, const connack_props& cprops = {}
) {
    constexpr int expected_handlers_called = 1;
    int handlers_called = 0;

    asio::io_context ioc;
    using client_service_type = test::test_service<asio::ip::tcp::socket>;
    auto svc_ptr = std::make_shared<client_service_type>(ioc.get_executor(), cprops);

    auto handler = [&handlers_called, expected_ec](error_code ec, reason_code rc, puback_props) {
        ++handlers_called;

        BOOST_TEST(ec == expected_ec);
        BOOST_TEST(rc == reason_codes::empty);
    };

    detail::publish_send_op<
        client_service_type, decltype(handler), qos_e::at_least_once
    > { svc_ptr, std::move(handler) }
        .perform(topic_name, payload, retain_e::yes, pprops);

    ioc.run_for(std::chrono::milliseconds(500));
    BOOST_TEST(handlers_called == expected_handlers_called);
}

BOOST_AUTO_TEST_CASE(invalid_topic_name_1) {
    run_test(client::error::invalid_topic, "");
}

BOOST_AUTO_TEST_CASE(invalid_topic_name_2) {
    run_test(client::error::invalid_topic, "+");
}

BOOST_AUTO_TEST_CASE(invalid_topic_name_3) {
    run_test(client::error::invalid_topic, "#");
}

BOOST_AUTO_TEST_CASE(invalid_topic_name_4) {
    run_test(client::error::invalid_topic, "invalid+");
}

BOOST_AUTO_TEST_CASE(invalid_topic_name_5) {
    run_test(client::error::invalid_topic, "invalid#");
}

BOOST_AUTO_TEST_CASE(invalid_topic_name_6) {
    run_test(client::error::invalid_topic, "invalid/#");
}

BOOST_AUTO_TEST_CASE(invalid_topic_name_7) {
    run_test(client::error::invalid_topic, "invalid/+");
}

void run_malformed_props_test(
    const publish_props& pprops, const connack_props& cprops = {}
) {
    return run_test(client::error::malformed_packet, "topic", "payload", pprops, cprops);
}

BOOST_AUTO_TEST_CASE(malformed_props_1) {
    publish_props pprops;
    pprops[prop::response_topic] = "response#topic";

    run_malformed_props_test(pprops);
}

BOOST_AUTO_TEST_CASE(malformed_props_2) {
    publish_props pprops;
    pprops[prop::user_property].emplace_back(std::string { 0x01 }, "value");

    run_malformed_props_test(pprops);
}

BOOST_AUTO_TEST_CASE(malformed_props_3) {
    publish_props pprops;
    pprops[prop::content_type] = std::string { 0x01 };

    run_malformed_props_test(pprops);
}

BOOST_AUTO_TEST_CASE(malformed_props_4) {
    connack_props cprops;
    cprops[prop::topic_alias_maximum] = uint16_t(10);

    publish_props pprops;
    pprops[prop::topic_alias] = uint16_t(0);

    run_malformed_props_test(pprops, cprops);
}

BOOST_AUTO_TEST_CASE(malformed_props_5) {
    publish_props pprops;
    pprops[prop::subscription_identifier].push_back(40);

    run_malformed_props_test(pprops);
}

BOOST_AUTO_TEST_CASE(malformed_utf8_payload) {
    publish_props pprops;
    pprops[prop::payload_format_indicator] = uint8_t(1);

    run_test(client::error::malformed_packet, "topic", std::string { 0x01 }, pprops);
}

BOOST_AUTO_TEST_CASE(packet_too_large) {
    connack_props cprops;
    cprops[prop::maximum_packet_size] = 10;

    run_test(
        client::error::packet_too_large,
        "very large topic", "very large payload",
        publish_props {}, cprops
    );
}

BOOST_AUTO_TEST_CASE(qos_not_supported) {
    connack_props cprops;
    cprops[prop::maximum_qos] = uint8_t(0);

    run_test(
        client::error::qos_not_supported,
        "topic", "payload", publish_props {}, cprops
    );
}

BOOST_AUTO_TEST_CASE(retain_not_available) {
    connack_props cprops;
    cprops[prop::retain_available] = uint8_t(0);

    run_test(
        client::error::retain_not_available,
        "topic", "payload", publish_props {}, cprops
    );
}

BOOST_AUTO_TEST_CASE(topic_alias_maximum_reached_1) {
    connack_props cprops;
    cprops[prop::topic_alias_maximum] = uint16_t(10);

    publish_props pprops;
    pprops[prop::topic_alias] = uint16_t(12);

    run_test(
        client::error::topic_alias_maximum_reached,
        "topic", "payload", pprops, cprops
    );
}

BOOST_AUTO_TEST_CASE(topic_alias_maximum_reached_2) {
    connack_props cprops; /* not allowed */

    publish_props pprops;
    pprops[prop::topic_alias] = uint16_t(12);

    run_test(
        client::error::topic_alias_maximum_reached,
        "topic", "payload", pprops, cprops
    );
}

BOOST_AUTO_TEST_SUITE_END()
