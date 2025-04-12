//
// Copyright (c) 2023-2025 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

//[multiflight_client
#include <boost/mqtt5/logger.hpp>
#include <boost/mqtt5/mqtt_client.hpp>
#include <boost/mqtt5/reason_codes.hpp>
#include <boost/mqtt5/types.hpp>

#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>

#include <iostream>
#include <string>

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

    client.brokers(cfg.brokers, cfg.port) // Broker that we want to connect to.
        .credentials(cfg.client_id) // Set the Client Identifier. (optional)
        .async_run(boost::asio::detached); // Start the client.

    // Publish with QoS 2 five times in a row without waiting for the handler
    // of the previous async_publish call to be invoked.
    for (auto i = 1; i <= 5; ++i)
        client.async_publish<boost::mqtt5::qos_e::exactly_once>(
            "boost-mqtt5/test", "Hello world!",
            boost::mqtt5::retain_e::no, boost::mqtt5::publish_props {},
            [i](boost::mqtt5::error_code ec, boost::mqtt5::reason_code rc, boost::mqtt5::pubcomp_props) {
                std::cout << "Publish number " << i << " completed with: " << std::endl;
                std::cout << "\t ec: " << ec.message() << std::endl;
                std::cout << "\t rc: " << rc.message() << std::endl;
            }
        );

    // We can stop the Client by using signals.
    boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait([&client](boost::mqtt5::error_code, int) {
        client.async_disconnect(boost::asio::detached);
    });

    ioc.run();
}
//]
