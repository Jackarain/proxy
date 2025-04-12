//
// Copyright (c) 2023-2025 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/use_awaitable.hpp>
#ifdef BOOST_ASIO_HAS_CO_AWAIT

//[publisher
#include <boost/mqtt5/logger.hpp>
#include <boost/mqtt5/mqtt_client.hpp>
#include <boost/mqtt5/reason_codes.hpp>
#include <boost/mqtt5/types.hpp>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/steady_timer.hpp>

#include <chrono>
#include <cstdlib>
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

int next_sensor_reading() {
    srand(static_cast<unsigned int>(std::time(0)));
    return rand() % 100;
}

boost::asio::awaitable<void> publish_sensor_readings(
    const config& cfg, client_type& client, boost::asio::steady_timer& timer
) {
    // Configure the Client.
    // It is mandatory to call brokers() and async_run() to configure the Brokers to connect to and start the Client.
    client.brokers(cfg.brokers, cfg.port) // Broker that we want to connect to.
        .credentials(cfg.client_id) // Set the Client Identifier. (optional)
        .async_run(boost::asio::detached); // Start the Client.

    for (;;) {
        // Get the next sensor reading.
        auto reading = std::to_string(next_sensor_reading());

        // Publish the sensor reading with QoS 1.
        auto&& [ec, rc, props] = co_await client.async_publish<boost::mqtt5::qos_e::at_least_once>(
            "boost-mqtt5/test" /* topic */, reading /* payload */,
            boost::mqtt5::retain_e::no, boost::mqtt5::publish_props {}, use_nothrow_awaitable
        );
        // An error can occur as a result of:
        //  a) wrong publish parameters
        //  b) mqtt_client::cancel is called while the Client is publishing the message
        //     resulting in cancellation.
        if (ec) {
            std::cout << "Publish error occurred: " << ec.message() << std::endl;
            break;
        }

        // Reason code is the reply from the server presenting the result of the publish operation.
        std::cout << "Result of publish request: " << rc.message() << std::endl;
        if (!rc)
            std::cout << "Published sensor reading: " << reading << std::endl;

        // Wait 5 seconds before publishing the next reading.
        timer.expires_after(std::chrono::seconds(5));
        auto&& [tec] = co_await timer.async_wait(use_nothrow_awaitable);

        // An error occurred if we cancelled the timer.
        if (tec)
            break;
    }

    co_return;
}

int main(int argc, char** argv) {
    config cfg;

    if (argc == 4) {
        cfg.brokers = argv[1];
        cfg.port = uint16_t(std::stoi(argv[2]));
        cfg.client_id = argv[3];
    }

    // Initialise execution context.
    boost::asio::io_context ioc;

    // Initialise the Client to connect to the Broker over TCP.
    client_type client(ioc, {} /* tls_context */, boost::mqtt5::logger(boost::mqtt5::log_level::info));

    // Initialise the timer.
    boost::asio::steady_timer timer(ioc);

    // Set up signals to stop the program on demand.
    boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait([&client, &timer](boost::mqtt5::error_code /* ec */, int /* signal */) {
        // After we are done with publishing all the messages, cancel the timer and the Client.
        // Alternatively, use mqtt_client::async_disconnect.
        timer.cancel();
        client.cancel();
    });

    // Spawn the coroutine.
    co_spawn(
        ioc.get_executor(),
        publish_sensor_readings(cfg, client, timer),
        [](std::exception_ptr e) {
            if (e)
                std::rethrow_exception(e);
        }
    );

    // Start the execution.
    ioc.run();
}

//]

#else

#include <iostream>

int main() {
    std::cout << "This example requires C++20 standard to compile and run" << std::endl;
}

#endif
