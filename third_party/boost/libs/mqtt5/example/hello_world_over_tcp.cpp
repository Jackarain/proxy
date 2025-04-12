//
// Copyright (c) 2023-2025 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

//[hello_world_over_tcp
#include <boost/mqtt5/logger.hpp>
#include <boost/mqtt5/mqtt_client.hpp>
#include <boost/mqtt5/types.hpp>

#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <iostream>
#include <string>

struct config {
    std::string brokers = "broker.hivemq.com";
    uint16_t port = 1883; // 1883 is the default TCP MQTT port.
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

    //[init_tcp_client_with_logger
    // Construct the Client with ``__TCP_SOCKET__`` as the underlying stream and enabled logging.
    // Since we are not establishing a secure connection, set the TlsContext template parameter to std::monostate.
    boost::mqtt5::mqtt_client<
        boost::asio::ip::tcp::socket, std::monostate /* TlsContext */, boost::mqtt5::logger
    > client(ioc, {} /* tls_context */, boost::mqtt5::logger(boost::mqtt5::log_level::info));
    //]

    // If you want to use the Client without logging, initialise it with the following line instead.
    //boost::mqtt5::mqtt_client<boost::asio::ip::tcp::socket> client(ioc);

    //[configure_tcp_client
    client.brokers(cfg.brokers, cfg.port) // Set the Broker to connect to.
        .credentials(cfg.client_id) // Set the Client Identifier. (optional)
        .async_run(boost::asio::detached); // Start the Client.
    //]

    //[publish_hello_world
    client.async_publish<boost::mqtt5::qos_e::at_most_once>(
        "boost-mqtt5/test", "Hello world!",
        boost::mqtt5::retain_e::yes, boost::mqtt5::publish_props {},
        [&client](boost::mqtt5::error_code ec) {
            std::cout << ec.message() << std::endl;

            // Disconnnect the Client.
            client.async_disconnect(boost::asio::detached);
        }
    );
    //]

    ioc.run();
}
//]
