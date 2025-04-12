//
// Copyright (c) 2023-2025 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mqtt5/mqtt_client.hpp>
#include <boost/mqtt5/types.hpp>

#include <boost/mqtt5/detail/traits.hpp>

#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/type_traits/remove_cv_ref.hpp>

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

#include "test_common/extra_deps.hpp"
#include "test_common/message_exchange.hpp"
#include "test_common/packet_util.hpp"
#include "test_common/test_authenticators.hpp"
#include "test_common/test_service.hpp"
#include "test_common/test_stream.hpp"

using namespace boost::mqtt5;
using namespace std::chrono_literals;

BOOST_AUTO_TEST_SUITE(client_functions)

struct shared_test_data {
    error_code success {};

    const std::string connack = encoders::encode_connack(
        false, reason_codes::success.value(), {}
    );
};

using client_type = mqtt_client<test::test_stream>;

template <typename TestingClientFun>
void run_test(
    test::msg_exchange broker_side, TestingClientFun&& client_fun
) {
    asio::io_context ioc;
    auto executor = ioc.get_executor();
    auto& broker = asio::make_service<test::test_broker>(
        ioc, executor, std::move(broker_side)
    );

    client_type c(executor);
    client_fun(c);
    c.brokers("127.0.0.1")
        .async_run(asio::detached);

    asio::steady_timer timer(executor);
    timer.expires_after(700ms);
    timer.async_wait(
        [&c](error_code) { c.cancel(); }
    );

    ioc.run();
    BOOST_TEST(broker.received_all_expected());
}

using test::after;

BOOST_AUTO_TEST_CASE(create_client_with_executor) {
    asio::io_context ioc;
    client_type c(ioc.get_executor());
    BOOST_CHECK(c.get_executor() == ioc.get_executor());
}

BOOST_AUTO_TEST_CASE(create_client_with_execution_context) {
    asio::io_context ioc;
    client_type c(ioc);
    BOOST_CHECK(c.get_executor() == ioc.get_executor());
}

BOOST_FIXTURE_TEST_CASE(assign_credentials, shared_test_data) {
    std::string client_id = "client_id";
    std::string username = "username";
    std::string password = "password";

    auto connect = encoders::encode_connect(
        client_id, username, password, 60, false, {}, std::nullopt
    );

    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(0ms))
            .reply_with(connack, after(1ms));

    run_test(
        std::move(broker_side), 
        [&](client_type& c) {
            c.credentials(client_id, username, password);
        }
    );
}

BOOST_FIXTURE_TEST_CASE(assign_will, shared_test_data) {
    will w("topic", "message");
    std::optional<will> will_opt { std::move(w) };

    auto connect = encoders::encode_connect(
        "", std::nullopt, std::nullopt, 60, false, {}, will_opt
    );

    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(0ms))
            .reply_with(connack, after(1ms));

    run_test(
        std::move(broker_side),
        [](client_type& c) {
            // because copying is deleted
            will w("topic", "message");
            will w_moved = std::move(w); // move assignment coverage
            c.will(std::move(w_moved));
        }
    );
}

void assign_authenticator() {
    // Tests if the authenticator function compiles

    asio::io_context ioc;
    client_type c(ioc);
    c.authenticator(test::test_authenticator());
}

BOOST_FIXTURE_TEST_CASE(assign_keep_alive, shared_test_data) {
    uint16_t keep_alive = 120;

    auto connect = encoders::encode_connect(
        "", std::nullopt, std::nullopt, keep_alive, false, {}, std::nullopt
    );

    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(0ms))
            .reply_with(connack, after(1ms));

    run_test(
        std::move(broker_side),
        [&](client_type& c) {
            c.keep_alive(keep_alive);
        }
    );
}

struct shared_connect_prop_test_data : shared_test_data {
    const int session_expiry_interval = 40;
    const int16_t receive_maximum = 10123;
    const int maximum_packet_size = 1024;
    const uint16_t topic_alias_maximum = 12345;
    const uint8_t request_response_information = 1;
    const uint8_t request_problem_information = 0;
    const std::vector<std::pair<std::string, std::string>> user_properties =
        { { "key", "val" } };

    connect_props cprops = create_connect_props();

private:
    connect_props create_connect_props() {
        connect_props ret_props;
        ret_props[prop::session_expiry_interval] = session_expiry_interval;
        ret_props[prop::receive_maximum] = receive_maximum;
        ret_props[prop::maximum_packet_size] = maximum_packet_size;
        ret_props[prop::topic_alias_maximum] = topic_alias_maximum;
        ret_props[prop::request_response_information] = request_response_information;
        ret_props[prop::request_problem_information] = request_problem_information;
        ret_props[prop::user_property] = user_properties;
        return ret_props;
    }
};

BOOST_FIXTURE_TEST_CASE(connect_properties, shared_connect_prop_test_data) {
    auto connect = encoders::encode_connect(
        "", std::nullopt, std::nullopt, 60, false, cprops, std::nullopt
    );

    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(0ms))
            .reply_with(connack, after(1ms));

    run_test(
        std::move(broker_side),
        [&](client_type& c) {
            c.connect_properties(cprops);
        }
    );
}

BOOST_FIXTURE_TEST_CASE(connect_property, shared_connect_prop_test_data) {
    auto connect = encoders::encode_connect(
        "", std::nullopt, std::nullopt, 60, false, cprops, std::nullopt
    );

    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(0ms))
            .reply_with(connack, after(1ms));

    run_test(
        std::move(broker_side),
        [&](client_type& c) {
            c.connect_property(prop::session_expiry_interval, session_expiry_interval);
            c.connect_property(prop::receive_maximum, receive_maximum);
            c.connect_property(prop::maximum_packet_size, maximum_packet_size);
            c.connect_property(prop::topic_alias_maximum, topic_alias_maximum);
            c.connect_property(prop::request_response_information, request_response_information);
            c.connect_property(prop::request_problem_information, request_problem_information);
            c.connect_property(prop::user_property, user_properties);
        }
    );
}

struct shared_connack_prop_test_data {
    error_code success {};

    // data
    const uint8_t session_present = 1;

    const int32_t session_expiry_interval = 20;
    const int16_t receive_maximum = 2000;
    const uint8_t max_qos = 2;
    const uint8_t retain_available = 0;
    const int32_t maximum_packet_sz = 1024;
    const std::string assigned_client_id = "client_id";
    const uint16_t topic_alias_max = 128;
    const std::string reason_string = "reason string";
    const std::vector<std::pair<std::string, std::string>> user_properties =
        { { "key", "val" } };
    const uint8_t wildcard_sub = 1;
    const uint8_t sub_id = 1;
    const uint8_t shared_sub = 0;
    const int16_t server_keep_alive = 25;
    const std::string response_information = "info";
    const std::string server_reference = "server reference";
    const std::string authentication_method = "method";
    const std::string authentication_data = "data";

    connack_props cprops = create_connack_props();

    // packets
    const std::string connect = encoders::encode_connect(
        "", std::nullopt, std::nullopt, 60, false, {}, std::nullopt
    );
    const std::string connack = encoders::encode_connack(
        false, reason_codes::success.value(), cprops
    );

private:
    connack_props create_connack_props() {
        connack_props ret_props;
        ret_props[prop::session_expiry_interval] = session_expiry_interval;
        ret_props[prop::receive_maximum] = receive_maximum;
        ret_props[prop::maximum_qos] = max_qos;
        ret_props[prop::retain_available] = retain_available;
        ret_props[prop::maximum_packet_size] = maximum_packet_sz;
        ret_props[prop::assigned_client_identifier] = assigned_client_id;
        ret_props[prop::topic_alias_maximum] = topic_alias_max;
        ret_props[prop::reason_string] = reason_string;
        ret_props[prop::user_property] = user_properties;
        ret_props[prop::wildcard_subscription_available] = wildcard_sub;
        ret_props[prop::subscription_identifier_available] = sub_id;
        ret_props[prop::shared_subscription_available] = shared_sub;
        ret_props[prop::server_keep_alive] = server_keep_alive;
        ret_props[prop::response_information] = response_information;
        ret_props[prop::server_reference] = server_reference;
        ret_props[prop::authentication_method] = authentication_method;
        ret_props[prop::authentication_data] = authentication_data;
        return ret_props;
    }
};

template <typename TestingClientFun>
void run_test_with_post_fun(
    test::msg_exchange broker_side, TestingClientFun&& client_fun
) {
    asio::io_context ioc;
    auto executor = ioc.get_executor();
    auto& broker = asio::make_service<test::test_broker>(
        ioc, executor, std::move(broker_side)
    );

    client_type c(executor);
    c.brokers("127.0.0.1")
        .async_run(asio::detached);

    asio::steady_timer timer(executor);
    timer.expires_after(700ms);
    timer.async_wait(
        [&c, fun = std::forward<TestingClientFun>(client_fun)](error_code) {
            fun(c);
            c.cancel();
        }
    );

    ioc.run();

    BOOST_TEST(broker.received_all_expected());
}


BOOST_FIXTURE_TEST_CASE(connack_properties, shared_connack_prop_test_data) {
    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(0ms))
            .reply_with(connack, after(1ms));

    run_test_with_post_fun(
        std::move(broker_side),
        [&](client_type& c) {
            connack_props cprops_ = c.connack_properties();
            cprops_.visit([&](const auto& p, const auto& val) -> bool {
                BOOST_TEST_REQUIRE(p);
                if constexpr (detail::is_vector<decltype(val)>)
                    BOOST_TEST(val == cprops[p]);
                else {
                    BOOST_TEST_REQUIRE(val.has_value());
                    BOOST_TEST(*val == *cprops[p]);
                }
                return true;
            });
        }
    );
}

BOOST_FIXTURE_TEST_CASE(connack_property, shared_connack_prop_test_data) {
    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(0ms))
            .reply_with(connack, after(1ms));

    run_test_with_post_fun(
        std::move(broker_side),
        [&](client_type& c) {
            cprops.visit([&](const auto& p, const auto& val) -> bool{
                if constexpr (detail::is_vector<decltype(val)>)
                    BOOST_TEST(val == c.connack_property(p));
                else {
                    BOOST_TEST_REQUIRE(val.has_value());
                    BOOST_TEST(*val == *c.connack_property(p));
                }
                return true;
            });
        }
    );
}

#ifdef BOOST_MQTT5_EXTRA_DEPS

void assign_credentials_tls_client() {
    // Tests if the assign credentials function compiles

    std::string client_id = "client_id";
    std::string username = "username";
    std::string password = "password";

    asio::io_context ioc;

    asio::ssl::context ctx(asio::ssl::context::tls_client);
    mqtt_client<
        asio::ssl::stream<asio::ip::tcp::socket>, asio::ssl::context
    > ts(ioc.get_executor(), std::move(ctx));

    ts.credentials(client_id, username, password);
}

void assign_tls_context() {
    // Tests if the tls_context function compiles

    asio::io_context ioc;
    asio::ssl::context ctx(asio::ssl::context::tls_client);

    mqtt_client<
        asio::ssl::stream<asio::ip::tcp::socket>, asio::ssl::context
    > tls_client(ioc.get_executor(), std::move(ctx));
    tls_client.tls_context();
}

void assign_authenticator_tls_client() {
    // Tests if the authenticator function compiles

    asio::io_context ioc;

    asio::ssl::context ctx(asio::ssl::context::tls_client);
    mqtt_client<
        asio::ssl::stream<asio::ip::tcp::socket>, asio::ssl::context
    > ts(ioc.get_executor(), std::move(ctx));

    ts.authenticator(test::test_authenticator());
}

BOOST_FIXTURE_TEST_CASE(connect_properties_tls_client, shared_connect_prop_test_data) {
    // Tests if the connect_properties function compiles

    asio::io_context ioc;

    asio::ssl::context ctx(asio::ssl::context::tls_client);
    mqtt_client<
        asio::ssl::stream<asio::ip::tcp::socket>, asio::ssl::context
    > ts(ioc.get_executor(), std::move(ctx));

    ts.connect_properties(cprops);
}

BOOST_FIXTURE_TEST_CASE(connect_property_tls_client, shared_connect_prop_test_data) {
    // Tests if the connect_property functions compile

    asio::io_context ioc;

    asio::ssl::context ctx(asio::ssl::context::tls_client);
    mqtt_client<
        asio::ssl::stream<asio::ip::tcp::socket>, asio::ssl::context
    > ts(ioc.get_executor(), std::move(ctx));

    ts.connect_property(prop::session_expiry_interval, session_expiry_interval);
    ts.connect_property(prop::receive_maximum, receive_maximum);
    ts.connect_property(prop::maximum_packet_size, maximum_packet_size);
    ts.connect_property(prop::topic_alias_maximum, topic_alias_maximum);
    ts.connect_property(prop::request_response_information, request_response_information);
    ts.connect_property(prop::request_problem_information, request_problem_information);
    ts.connect_property(prop::user_property, user_properties);
}

void connack_property_with_tls_client() {
    // Tests if the connack_properties & connack_property functions compile

    asio::io_context ioc;

    asio::ssl::context ctx(asio::ssl::context::tls_client);
    mqtt_client<
        asio::ssl::stream<asio::ip::tcp::socket>, asio::ssl::context
    > ts(ioc.get_executor(), std::move(ctx));

    auto cprops_ = ts.connack_properties();
    cprops_.visit([&ts](const auto& p, auto&) -> bool {
        using ptype = boost::remove_cv_ref_t<decltype(p)>;
        [[maybe_unused]] prop::value_type_t<ptype::value> value = ts.connack_property(p);
        return true;
    });
}
#endif // BOOST_MQTT5_EXTRA_DEPS

BOOST_AUTO_TEST_SUITE_END();
