//
// Copyright (c) 2023-2025 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

//[hello_world_over_websocket_tcp
#include <boost/mqtt5/logger.hpp>
#include <boost/mqtt5/mqtt_client.hpp>
#include <boost/mqtt5/types.hpp>
#include <boost/mqtt5/websocket.hpp> // WebSocket traits

#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/websocket.hpp>

#include <iostream>
#include <string>

struct config {
    std::string brokers = "broker.hivemq.com/mqtt"; // Path example: localhost/mqtt
    uint16_t port = 8000; // 8083 is the default Webscoket/TCP MQTT port. However, HiveMQ's public broker uses 8000 instead.
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

    // Construct the Client with WebSocket/TCP as the underlying stream and enabled logging.
    // Since we are not establishing a secure connection, set the TlsContext template parameter to std::monostate.
    boost::mqtt5::mqtt_client<
        boost::beast::websocket::stream<boost::asio::ip::tcp::socket>,
        std::monostate,
        boost::mqtt5::logger
    > client(ioc, {}, boost::mqtt5::logger(boost::mqtt5::log_level::info));

    // If you want to use the Client without logging, initialise it with the following line instead.
    //boost::mqtt5::mqtt_client<boost::beast::websocket::stream<boost::asio::ip::tcp::socket>> client(ioc);

    client.brokers(cfg.brokers, cfg.port) // Broker that we want to connect to.
        .credentials(cfg.client_id) // Set the Client Identifier. (optional)
        .async_run(boost::asio::detached); // Start the Client.

    client.async_publish<boost::mqtt5::qos_e::at_most_once>(
        "boost-mqtt5/test", "Hello world!",
        boost::mqtt5::retain_e::no, boost::mqtt5::publish_props {},
        [&client](boost::mqtt5::error_code ec) {
            std::cout << ec.message() << std::endl;
            client.async_disconnect(boost::asio::detached);
        }
    );

    ioc.run();
}
//]
