//
// Copyright (c) 2023-2025 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/test/unit_test.hpp>

#include <boost/asio/use_awaitable.hpp>
#ifdef BOOST_ASIO_HAS_CO_AWAIT

#include <boost/mqtt5.hpp>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>

#include <chrono>
#include <cstdint>
#include <string>

#include "test_common/preconditions.hpp"

BOOST_AUTO_TEST_SUITE(mqtt_features,
    * boost::unit_test::precondition(boost::mqtt5::test::public_broker_cond))

using namespace boost::mqtt5;
namespace asio = boost::asio;

constexpr auto use_nothrow_awaitable = asio::as_tuple(asio::use_awaitable);

constexpr auto test_duration = std::chrono::seconds(5);

using stream_type = asio::ip::tcp::socket;

constexpr auto broker = "broker.hivemq.com";
constexpr auto connect_wait_dur = std::chrono::milliseconds(200);
constexpr auto topic = "boost-mqtt5/test";
constexpr auto share_topic = "$share/sharename/boost-mqtt5/test";
constexpr auto payload = "hello from boost-mqtt5";

template <typename TestCase>
void run_test(TestCase&& test_case) {
    using namespace asio::experimental::awaitable_operators;

    asio::io_context ioc;
    co_spawn(
        ioc,
        [&ioc, test_case = std::forward<TestCase>(test_case)]() -> asio::awaitable<void> {
            asio::steady_timer test_timer(ioc, test_duration);
            co_await(test_case() || test_timer.async_wait(use_nothrow_awaitable));
        },
        asio::detached
    );
    ioc.run();
}

asio::awaitable<void> test_manual_use_topic_alias() {
    auto ex = co_await asio::this_coro::executor;

    mqtt_client<stream_type> client(ex);
    client.brokers(broker, 1883)
        .connect_property(prop::topic_alias_maximum, uint16_t(10))
        .async_run(asio::detached);

    asio::steady_timer connect_timer(ex, connect_wait_dur);
    co_await connect_timer.async_wait(use_nothrow_awaitable);

    uint16_t topic_alias = 1;
    publish_props pprops;
    pprops[prop::topic_alias] = topic_alias;

    auto&& [ec_1, rc_1, _] = co_await client.async_publish<qos_e::at_least_once>(
        topic, payload, retain_e::no, pprops, use_nothrow_awaitable
    );
    BOOST_TEST_WARN(!ec_1);
    BOOST_TEST_WARN(!rc_1);

    auto&& [ec_2, rc_2, __] = co_await client.async_publish<qos_e::at_least_once>(
        "", payload, retain_e::no, pprops, use_nothrow_awaitable
    );
    BOOST_TEST_WARN(!ec_2);
    BOOST_TEST_WARN(!rc_2);
}

BOOST_AUTO_TEST_CASE(manual_use_topic_alias) {
    run_test(test_manual_use_topic_alias);
}

asio::awaitable<void> test_subscription_identifiers() {
    auto ex = co_await asio::this_coro::executor;

    mqtt_client<stream_type> client(ex);
    client.brokers(broker, 1883)
        .async_run(asio::detached);

    publish_props pprops;
    auto&& [ec_1, rc_1, _] = co_await client.async_publish<qos_e::at_least_once>(
        topic, payload, retain_e::yes, pprops, use_nothrow_awaitable
    );
    BOOST_TEST_WARN(!ec_1);
    BOOST_TEST_WARN(!rc_1);

    int32_t sub_id = 123;
    subscribe_props sprops;
    sprops[prop::subscription_identifier] = sub_id;

    subscribe_options sub_opts = { .no_local = no_local_e::no };
    subscribe_topic sub_topic = { topic, sub_opts };
    auto&& [ec_2, rcs, __] = co_await client.async_subscribe(
        sub_topic, sprops, use_nothrow_awaitable
    );
    BOOST_TEST_WARN(!ec_2);
    BOOST_TEST_WARN(!rcs[0]);

    auto&& [ec_3, rec_topic, rec_payload, rec_props] =
        co_await client.async_receive(use_nothrow_awaitable);
    BOOST_TEST_WARN(!ec_3);
    BOOST_TEST_WARN(rec_topic == topic);
    BOOST_TEST_WARN(rec_payload == payload);
    const auto& sub_ids = rec_props[prop::subscription_identifier];
    BOOST_TEST_WARN(!sub_ids.empty());
    if (!sub_ids.empty())
        BOOST_TEST_WARN(sub_ids[0] == sub_id);
}

BOOST_AUTO_TEST_CASE(subscription_identifiers) {
    run_test(test_subscription_identifiers);
}

asio::awaitable<void> test_shared_subscription() {
    auto ex = co_await asio::this_coro::executor;

    mqtt_client<stream_type> client(ex);
    client.brokers(broker, 1883)
        .async_run(asio::detached);

    subscribe_options sub_opts = { .no_local = no_local_e::no };
    subscribe_topic sub_topic = { share_topic, sub_opts };
    subscribe_props sprops;
    auto&& [ec_1, rcs, __] = co_await client.async_subscribe(
        sub_topic, sprops, use_nothrow_awaitable
    );
    BOOST_TEST_WARN(!ec_1);
    BOOST_TEST_WARN(!rcs[0]);

    publish_props pprops;
    // shared subscriptions do not send Retained Messages on first subscribe
    auto&& [ec_2, rc_2, _] = co_await client.async_publish<qos_e::at_least_once>(
        topic, payload, retain_e::no, pprops, use_nothrow_awaitable
    );
    BOOST_TEST_WARN(!ec_2);
    BOOST_TEST_WARN(!rc_2);

    auto&& [ec_3, rec_topic, rec_payload, ___] =
        co_await client.async_receive(use_nothrow_awaitable);
    BOOST_TEST_WARN(!ec_3);
    BOOST_TEST_WARN(rec_topic == topic);
    BOOST_TEST_WARN(rec_payload == payload);
}

BOOST_AUTO_TEST_CASE(shared_subscription) {
    run_test(test_shared_subscription);
}

asio::awaitable<void> test_user_property() {
    auto ex = co_await asio::this_coro::executor;

    mqtt_client<stream_type> client(ex);
    client.brokers(broker, 1883)
        .async_run(asio::detached);

    publish_props pprops;
    pprops[prop::user_property].push_back({ "key_1", "value_1" });
    pprops[prop::user_property].push_back({ "key_2", "value_2" });
    pprops[prop::user_property].push_back({ "key_3", "value_3" });

    auto&& [ec_1, rc_1, _] = co_await client.async_publish<qos_e::at_least_once>(
        topic, payload, retain_e::no, pprops, use_nothrow_awaitable
    );
    BOOST_TEST_WARN(!ec_1);
    BOOST_TEST_WARN(!rc_1);
}

BOOST_AUTO_TEST_CASE(user_property) {
    run_test(test_user_property);
}

BOOST_AUTO_TEST_SUITE_END()

#endif
