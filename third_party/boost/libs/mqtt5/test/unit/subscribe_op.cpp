//
// Copyright (c) 2023-2025 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mqtt5/types.hpp>

#include <boost/mqtt5/impl/subscribe_op.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/test/unit_test.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "test_common/test_service.hpp"

using namespace boost::mqtt5;

BOOST_AUTO_TEST_SUITE(subscribe_op/*, *boost::unit_test::disabled()*/)

BOOST_AUTO_TEST_CASE(pid_overrun) {
    constexpr int expected_handlers_called = 1;
    int handlers_called = 0;

    asio::io_context ioc;
    using client_service_type = test::overrun_client<asio::ip::tcp::socket>;
    auto svc_ptr = std::make_shared<client_service_type>(ioc.get_executor());

    auto handler = [&handlers_called](error_code ec, std::vector<reason_code> rcs, suback_props) {
        ++handlers_called;
        BOOST_TEST(ec == client::error::pid_overrun);
        BOOST_TEST_REQUIRE(rcs.size() == 1u);
        BOOST_TEST(rcs[0] == reason_codes::empty);
    };

    detail::subscribe_op<
        client_service_type, decltype(handler)
    > { svc_ptr, std::move(handler) }
    .perform(
        { { "topic", { qos_e::exactly_once } } }, subscribe_props {}
    );

    ioc.run_for(std::chrono::milliseconds(500));
    BOOST_TEST(handlers_called == expected_handlers_called);
}

void run_test(
    error_code expected_ec, const std::vector<subscribe_topic>& topics,
    const subscribe_props& sprops = {}, const connack_props& cprops = {}
) {
    constexpr int expected_handlers_called = 1;
    int handlers_called = 0;

    asio::io_context ioc;
    using client_service_type = test::test_service<asio::ip::tcp::socket>;
    auto svc_ptr = std::make_shared<client_service_type>(ioc.get_executor(), cprops);

    auto handler = [&handlers_called, expected_ec, num_tp = topics.size()]
        (error_code ec, std::vector<reason_code> rcs, suback_props) {
            ++handlers_called;

            BOOST_TEST(ec == expected_ec);
            BOOST_TEST_REQUIRE(rcs.size() == num_tp);

            for (size_t i = 0; i < rcs.size(); ++i)
                BOOST_TEST(rcs[i] == reason_codes::empty);
        };

    detail::subscribe_op<
        client_service_type, decltype(handler)
    > { svc_ptr, std::move(handler) }
    .perform(topics, sprops);

    ioc.run_for(std::chrono::milliseconds(500));
    BOOST_TEST(handlers_called == expected_handlers_called);
}

void run_test(
    error_code expected_ec, const std::string& topic,
    const subscribe_props& sprops = {}, const connack_props& cprops = {}
) {
    auto sub_topic = subscribe_topic { topic, subscribe_options() };
    return run_test(
        expected_ec,
        std::vector<subscribe_topic> { std::move(sub_topic) },
        sprops, cprops
    );
}

BOOST_AUTO_TEST_CASE(invalid_topic_filter_1) {
    run_test(client::error::invalid_topic, "");
}

BOOST_AUTO_TEST_CASE(invalid_topic_filter_2) {
    run_test(client::error::invalid_topic, "+topic");
}

BOOST_AUTO_TEST_CASE(invalid_topic_filter_3) {
    run_test(client::error::invalid_topic, "topic+");
}

BOOST_AUTO_TEST_CASE(invalid_topic_filter_4) {
    run_test(client::error::invalid_topic, "#topic");
}

BOOST_AUTO_TEST_CASE(invalid_topic_filter_5) {
    run_test(client::error::invalid_topic, "some/#/topic");
}

BOOST_AUTO_TEST_CASE(invalid_topic_filter_6) {
    run_test(client::error::invalid_topic, "$share//topic#");
}

BOOST_AUTO_TEST_CASE(malformed_user_property_1) {
    subscribe_props sprops;
    sprops[prop::user_property].emplace_back("key", std::string(10, char(0x01)));

    run_test(client::error::malformed_packet, "topic", sprops);
}

BOOST_AUTO_TEST_CASE(malformed_user_property_2) {
    subscribe_props sprops;
    sprops[prop::user_property].emplace_back("key", std::string(75000, 'a'));

    run_test(client::error::malformed_packet, "topic", sprops);
}

BOOST_AUTO_TEST_CASE(wildcard_subscriptions_not_available_1) {
    connack_props cprops;
    cprops[prop::wildcard_subscription_available] = uint8_t(0);

    run_test(
        client::error::wildcard_subscription_not_available, "topic/#",
        subscribe_props {}, cprops
    );
}

BOOST_AUTO_TEST_CASE(wildcard_subscriptions_not_available_2) {
    connack_props cprops;
    cprops[prop::wildcard_subscription_available] = uint8_t(0);

    run_test(
        client::error::wildcard_subscription_not_available, "$share/grp/topic/#",
        subscribe_props {}, cprops
    );
}

BOOST_AUTO_TEST_CASE(shared_subscriptions_not_available) {
    connack_props cprops;
    cprops[prop::shared_subscription_available] = uint8_t(0);

    run_test(
        client::error::shared_subscription_not_available, "$share/group/topic",
        subscribe_props {}, cprops
    );
}

BOOST_AUTO_TEST_CASE(subscription_id_not_available) {
    connack_props cprops;
    cprops[prop::subscription_identifier_available] = uint8_t(0);

    subscribe_props sprops;
    sprops[prop::subscription_identifier] = 23;

    run_test(
        client::error::subscription_identifier_not_available, "topic", sprops, cprops
    );
}

BOOST_AUTO_TEST_CASE(large_subscription_id) {
    connack_props cprops;
    cprops[prop::subscription_identifier_available] = uint8_t(1);

    subscribe_props sprops;
    sprops[prop::subscription_identifier] = (std::numeric_limits<int32_t>::max)();

    run_test(client::error::malformed_packet, "topic", sprops, cprops);
}

BOOST_AUTO_TEST_CASE(packet_too_large) {
    connack_props cprops;
    cprops[prop::maximum_packet_size] = 10;

    run_test(
        client::error::packet_too_large, "very large topic", subscribe_props {}, cprops
    );
}

BOOST_AUTO_TEST_CASE(zero_topic_filters) {
    run_test(client::error::invalid_topic, std::vector<subscribe_topic> {});
}

BOOST_AUTO_TEST_SUITE_END()
