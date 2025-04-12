//
// Copyright (c) 2023-2025 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/use_awaitable.hpp>
#ifdef BOOST_ASIO_HAS_CO_AWAIT

//[timeout_with_awaitable_operators

#include <boost/mqtt5/logger.hpp>
#include <boost/mqtt5/mqtt_client.hpp>
#include <boost/mqtt5/reason_codes.hpp>
#include <boost/mqtt5/types.hpp>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>

#include <chrono>
#include <iostream>
#include <string>

struct config {
    std::string brokers = "broker.hivemq.com";
    uint16_t port = 1883;
    std::string client_id = "boost_mqtt5_tester";
};

// Modified completion token that will prevent co_await from throwing exceptions.
constexpr auto use_nothrow_awaitable = boost::asio::as_tuple(boost::asio::deferred);

// client_type with logging enabled
using client_type = boost::mqtt5::mqtt_client<
    boost::asio::ip::tcp::socket, std::monostate /* TlsContext */, boost::mqtt5::logger
>;

// client_type without logging
//using client_type = boost::mqtt5::mqtt_client<boost::asio::ip::tcp::socket>;

boost::asio::awaitable<void> send_over_mqtt(
    const config& cfg, client_type& client
) {
    client.brokers(cfg.brokers, cfg.port) // Broker that we want to connect to.
        .credentials(cfg.client_id) // Set the Client Identifier. (optional)
        .async_run(boost::asio::detached); // Start the Client.

    auto&& [pec, prc, puback_props] = co_await client.async_publish<boost::mqtt5::qos_e::at_least_once>(
        "boost-mqtt5/test", "Hello World!",
        boost::mqtt5::retain_e::no, boost::mqtt5::publish_props {},
        use_nothrow_awaitable
    );

    co_await client.async_disconnect(use_nothrow_awaitable);
    co_return;
}

int main(int argc, char** argv) {
    config cfg;

    if (argc == 4) {
        cfg.brokers = argv[1];
        cfg.port = uint16_t(std::stoi(argv[2]));
        cfg.client_id = argv[3];
    }

    boost::asio::io_context ioc;

    co_spawn(
        ioc,
        [&ioc, &cfg]() -> boost::asio::awaitable<void> {
        // Initialise the Client to connect to the Broker over TCP.
        client_type client(ioc);

        // You can also initialise the Client and its logger with a specific log_level (default log_level::info).
        //client_type client(ioc, {} /* tls_context */, boost::mqtt5::logger(boost::mqtt5::log_level::debug));

        // Construct the timer.
        boost::asio::steady_timer timer(ioc, std::chrono::seconds(5));

        using namespace boost::asio::experimental::awaitable_operators;
        auto res = co_await (
            send_over_mqtt(cfg, client) ||
            timer.async_wait(boost::asio::as_tuple(boost::asio::use_awaitable))
        );

        // The timer expired first. The client is cancelled.
        if (res.index() == 1)
            std::cout << "Send over MQTT timed out!" << std::endl;
        // send_over_mqtt completed first. The timer is cancelled.
        else
            std::cout << "Send over MQTT completed!" << std::endl;
        
        },
        [](std::exception_ptr e) {
            if (e)
                std::rethrow_exception(e);
        }
    );

    ioc.run();
}

//]

#else

#include <iostream>

int main() {
    std::cout << "This example requires C++20 standard to compile and run" << std::endl;
}

#endif
