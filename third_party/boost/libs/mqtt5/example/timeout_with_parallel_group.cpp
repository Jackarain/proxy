//
// Copyright (c) 2023-2025 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

//[timeout_with_parallel_group
#include <boost/mqtt5/logger.hpp>
#include <boost/mqtt5/mqtt_client.hpp>
#include <boost/mqtt5/reason_codes.hpp>
#include <boost/mqtt5/types.hpp>

#include <boost/asio/deferred.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>

#include <array>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

struct config {
    std::string brokers = "broker.hivemq.com";
    uint16_t port = 1883;
    std::string client_id = "boost_mqtt5_tester";
};

int main(int argc, char** argv) {
    config cfg;

    if (argc == 4) {
        cfg.brokers = argv[1];
        cfg.port = uint16_t(std::stoi(argv[2]));
        cfg.client_id = argv[3];
    }

    boost::asio::io_context ioc;

    // Construct the Client with ``__TCP_SOCKET__`` as the underlying stream and enabled logging.
    // Since we are not establishing a secure connection, set the TlsContext template parameter to std::monostate.
    boost::mqtt5::mqtt_client<
        boost::asio::ip::tcp::socket, std::monostate /* TlsContext */, boost::mqtt5::logger
    > client(ioc, {} /* tls_context */, boost::mqtt5::logger(boost::mqtt5::log_level::info));

    // If you want to use the Client without logging, initialise it with the following line instead.
    //boost::mqtt5::mqtt_client<boost::asio::ip::tcp::socket> client(ioc);

    // Construct the timer.
    boost::asio::steady_timer timer(ioc, std::chrono::seconds(5));

    client.brokers(cfg.brokers, cfg.port) // Broker that we want to connect to.
        .credentials(cfg.client_id) // Set the Client Identifier. (optional)
        .async_run(boost::asio::detached); // Start the Client.

    // Subscribe to a Topic.
    client.async_subscribe(
        { "test" /* Topic */}, boost::mqtt5::subscribe_props {},
        [](boost::mqtt5::error_code ec, std::vector<boost::mqtt5::reason_code> rcs, boost::mqtt5::suback_props) {
            std::cout << "[subscribe ec]: " << ec.message() << std::endl;
            std::cout << "[subscribe rc]: " << rcs[0].message() << std::endl;
        }
    );

    // Create a parallel group to wait up to 5 seconds to receive a message
    // using client.async_receive(...).
    boost::asio::experimental::make_parallel_group(
        timer.async_wait(boost::asio::deferred),
        client.async_receive(boost::asio::deferred)
    ).async_wait(
        boost::asio::experimental::wait_for_one(),
        [&client](
            std::array<std::size_t, 2> ord, // Completion order
            boost::mqtt5::error_code /* timer_ec */, // timer.async_wait(...) handler signature
            // client.async_receive(...) handler signature
            boost::mqtt5::error_code receive_ec, 
            std::string topic, std::string payload, boost::mqtt5::publish_props /* props */
        ) {
            if (ord[0] == 1) {
                std::cout << "Received a message!" << std::endl;
                std::cout << "[receive ec]: " << receive_ec.message() << std::endl;
                std::cout << "[receive topic]: " << topic << std::endl;
                std::cout << "[receive payload]: " << payload << std::endl;
            }
            else
                std::cout << "Timed out! Did not receive a message within 5 seconds." << std::endl;

            client.cancel();
        }
    );

    ioc.run();
}

//]
