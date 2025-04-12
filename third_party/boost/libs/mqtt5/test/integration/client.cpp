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
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/error_code.hpp>

#include <chrono>

#include "test_common/extra_deps.hpp"
#include "test_common/preconditions.hpp"

BOOST_AUTO_TEST_SUITE(client,
    * boost::unit_test::precondition(boost::mqtt5::test::public_broker_cond))

using namespace boost::mqtt5;
namespace asio = boost::asio;

constexpr auto use_nothrow_awaitable = asio::as_tuple(asio::use_awaitable);

template<typename StreamType, typename TlsContext>
asio::awaitable<void> test_client(mqtt_client<StreamType, TlsContext>& c) {
    // Note: Older versions of GCC compilers may not handle temporaries
    // correctly in co_await expressions.
    // (https://gcc.gnu.org/bugzilla/show_bug.cgi?id=98401)
    publish_props pub_props;
    auto [ec_0] = co_await c.template async_publish<qos_e::at_most_once>(
        "test/mqtt-test", "hello world with qos0!", retain_e::yes, pub_props,
        use_nothrow_awaitable
    );
    BOOST_TEST_WARN(!ec_0);

    auto [ec_1, puback_rc, puback_props] = co_await c.template async_publish<qos_e::at_least_once>(
        "test/mqtt-test", "hello world with qos1!",
        retain_e::yes, pub_props,
        use_nothrow_awaitable
    );
    BOOST_TEST_WARN(!ec_1);
    BOOST_TEST_WARN(!puback_rc);

    auto [ec_2, pubcomp_rc, pubcomp_props] = co_await c.template async_publish<qos_e::exactly_once>(
        "test/mqtt-test", "hello world with qos2!",
        retain_e::yes, pub_props,
        use_nothrow_awaitable
    );
    BOOST_TEST_WARN(!ec_2);
    BOOST_TEST_WARN(!pubcomp_rc);

    subscribe_topic sub_topic = subscribe_topic {
        "test/mqtt-test", boost::mqtt5::subscribe_options {
            qos_e::at_least_once,
            no_local_e::no,
            retain_as_published_e::retain,
            retain_handling_e::send
        }
    };
    
    subscribe_props sub_props;
    auto [sub_ec, sub_codes, suback_props] = co_await c.async_subscribe(
        sub_topic, sub_props, use_nothrow_awaitable
    );
    BOOST_TEST_WARN(!sub_ec);
    if (!sub_codes[0])
        auto [rec, topic, payload, publish_props] = co_await c.async_receive(use_nothrow_awaitable);

    unsubscribe_props unsub_props;
    auto [unsub_ec, unsub_codes, unsuback_props] = co_await c.async_unsubscribe(
        "test/mqtt-test", unsub_props,
        use_nothrow_awaitable
    );
    BOOST_TEST_WARN(!unsub_ec);
    BOOST_TEST_WARN(!unsub_codes[0]);

    co_await c.async_disconnect(use_nothrow_awaitable);
    co_return;
}


BOOST_AUTO_TEST_CASE(tcp_client_check) {
    asio::io_context ioc;

    using stream_type = asio::ip::tcp::socket;
    using client_type = mqtt_client<stream_type>;
    client_type c(ioc);

    c.brokers("broker.hivemq.com", 1883)
        .async_run(asio::detached);

    asio::steady_timer timer(ioc);
    timer.expires_after(std::chrono::seconds(5));

    timer.async_wait(
        [&](boost::system::error_code ec) {
            BOOST_TEST_WARN(ec, "Failed to receive all the expected replies!");
            c.cancel();
        }
    );

    co_spawn(ioc, 
        [&]() -> asio::awaitable<void> {
            co_await test_client(c);
            timer.cancel();
        },
        asio::detached
    );

    ioc.run();
}

#ifdef BOOST_MQTT5_EXTRA_DEPS

BOOST_AUTO_TEST_CASE(websocket_tcp_client_check) {
    asio::io_context ioc;

    using stream_type = boost::beast::websocket::stream<
        asio::ip::tcp::socket
    >;

    using client_type = mqtt_client<stream_type>;
    client_type c(ioc);

    c.brokers("broker.hivemq.com/mqtt", 8000)
        .async_run(asio::detached);

    asio::steady_timer timer(ioc);
    timer.expires_after(std::chrono::seconds(5));

    timer.async_wait(
        [&](boost::system::error_code ec) {
            BOOST_TEST_WARN(ec, "Failed to receive all the expected replies!");
            c.cancel();
        }
    );

    co_spawn(ioc,
        [&]() -> asio::awaitable<void> {
            co_await test_client(c);
            timer.cancel();
        },
        asio::detached
    );

    ioc.run();
}


BOOST_AUTO_TEST_CASE(openssl_tls_client_check) {
    asio::io_context ioc;

    using stream_type = asio::ssl::stream<asio::ip::tcp::socket>;
    asio::ssl::context tls_context(asio::ssl::context::tls_client);

    using client_type = mqtt_client<stream_type, decltype(tls_context)>;
    client_type c(ioc, std::move(tls_context));

    c.brokers("broker.hivemq.com", 8883)
        .async_run(asio::detached);

    asio::steady_timer timer(ioc);
    timer.expires_after(std::chrono::seconds(5));

    timer.async_wait(
        [&](boost::system::error_code ec) {
            BOOST_TEST_WARN(ec, "Failed to receive all the expected replies!");
            c.cancel();
        }
    );

    co_spawn(ioc,
         [&]() -> asio::awaitable<void> {
             co_await test_client(c);
             timer.cancel();
         },
         asio::detached
    );

    ioc.run();
}

BOOST_AUTO_TEST_CASE(websocket_tls_client_check) {
    asio::io_context ioc;

    using stream_type = boost::beast::websocket::stream<
        asio::ssl::stream<asio::ip::tcp::socket>
    >;

    asio::ssl::context tls_context(asio::ssl::context::tls_client);

    using client_type = mqtt_client<stream_type, decltype(tls_context)>;
    client_type c(ioc, std::move(tls_context));

    c.brokers("broker.hivemq.com/mqtt", 8884)
        .async_run(asio::detached);

    asio::steady_timer timer(ioc);
    timer.expires_after(std::chrono::seconds(5));

    timer.async_wait(
        [&](boost::system::error_code ec) {
            BOOST_TEST_WARN(ec, "Failed to receive all the expected replies!");
            c.cancel();
        }
    );

    co_spawn(ioc,
        [&]() -> asio::awaitable<void> {
            co_await test_client(c);
            timer.cancel();
        },
        asio::detached
    );

    ioc.run();
}
#endif // BOOST_MQTT5_EXTRA_DEPS

BOOST_AUTO_TEST_SUITE_END()

#endif // BOOST_ASIO_HAS_CO_AWAIT
