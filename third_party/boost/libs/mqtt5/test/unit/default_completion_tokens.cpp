//
// Copyright (c) 2023-2025 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/use_awaitable.hpp>

#ifdef BOOST_ASIO_HAS_CO_AWAIT

#include <boost/mqtt5.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <cstdint>
#include <string>
#include <variant> // std::monostate
#include <vector>

#include "test_common/extra_deps.hpp"

namespace boost::mqtt5::test {

// the following code needs to compile

template <
    typename StreamType,
    typename TlsContextType = std::monostate,
    typename Logger = noop_logger
>
asio::awaitable<void> test_default_completion_tokens_impl(
    TlsContextType tls_context = {}, Logger logger = {}
) {
    asio::io_context ioc;

    using client_type = asio::use_awaitable_t<>::as_default_on_t<
        mqtt_client<StreamType, TlsContextType, Logger>
    >;
    client_type c(ioc, std::move(tls_context), std::move(logger));

    co_await c.async_run();
    
    auto pub_props = publish_props {};
    co_await c.template async_publish<qos_e::at_least_once>(
        "topic", "payload", retain_e::no, pub_props
    );

    auto sub_topic = subscribe_topic {};
    auto sub_topics = std::vector<subscribe_topic> { sub_topic };
    auto sub_props = subscribe_props {};
    co_await c.async_subscribe(sub_topics, sub_props);
    co_await c.async_subscribe(sub_topic, sub_props);

    auto unsub_topics = std::vector<std::string> {};
    auto unsub_props = unsubscribe_props {};
    co_await c.async_unsubscribe(unsub_topics, unsub_props);
    co_await c.async_unsubscribe("topic", unsub_props);

    co_await c.async_receive();

    auto dc_props = disconnect_props {};
    co_await c.async_disconnect();
    co_await c.async_disconnect(disconnect_rc_e::normal_disconnection, dc_props);
    co_return;
}

asio::awaitable<void> test_default_completion_tokens() {
    co_await test_default_completion_tokens_impl<asio::ip::tcp::socket>();

    #ifdef BOOST_MQTT5_EXTRA_DEPS
    co_await test_default_completion_tokens_impl<
        asio::ssl::stream<asio::ip::tcp::socket>,
        asio::ssl::context
    >(asio::ssl::context(asio::ssl::context::tls_client));

    co_await test_default_completion_tokens_impl<
        boost::beast::websocket::stream<asio::ip::tcp::socket>
    >();

    co_await test_default_completion_tokens_impl<
        boost::beast::websocket::stream<asio::ssl::stream<asio::ip::tcp::socket>>,
        asio::ssl::context,
        logger
    >(
        asio::ssl::context(asio::ssl::context::tls_client),
        logger(log_level::debug)
    );
    #endif // BOOST_MQTT5_EXTRA_DEPS
}

} // end namespace boost::mqtt5::test

#endif // BOOST_ASIO_HAS_CO_AWAIT
