//
// Copyright (c) 2023-2025 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

//[hello_world_in_multithreaded_env
#include <boost/mqtt5/logger.hpp>
#include <boost/mqtt5/mqtt_client.hpp>
#include <boost/mqtt5/reason_codes.hpp>
#include <boost/mqtt5/types.hpp>

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/thread_pool.hpp>

#include <cassert>
#include <iostream>
#include <string>
#include <thread>
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

    // Create a thread pool with 4 threads.
    boost::asio::thread_pool tp(4);
    
    // Create an explicit strand from thread_pool's executor.
    // The strand guarantees a serialised handler execution regardless of the 
    // number of threads running in the thread_pool.
    boost::asio::strand strand = boost::asio::make_strand(tp.get_executor());

    // Create the Client with the explicit strand as the default associated executor.
    boost::mqtt5::mqtt_client<
        boost::asio::ip::tcp::socket, std::monostate /* TlsContext */, boost::mqtt5::logger
    > client(strand, {} /* tls_context */, boost::mqtt5::logger(boost::mqtt5::log_level::info));

    // Configure the client.
    client.brokers(cfg.brokers, cfg.port) // Broker that we want to connect to.
        .credentials(cfg.client_id); // Set the Client Identifier. (optional)

    // Start the Client.
    // The async_run function call must be posted/dispatched to the strand.
    boost::asio::dispatch(
        boost::asio::bind_executor(
            strand,
            [&client, &strand, &cfg] { 
                // Considering that the default associated executor of all completion handlers is the strand,
                // it is not necessary to explicitly bind it to async_run or other async_xxx's handlers.
                client.async_run(boost::asio::detached); // Start the Client.
            }
        )
    );
    
    // The async_publish function call must be posted/dispatched to the strand.
    // The associated executor of async_publish's completion handler must be the same strand.
    boost::asio::dispatch(
        boost::asio::bind_executor(
            strand,
            [&client, &strand, &cfg] { 
                assert(strand.running_in_this_thread());

                client.async_publish<boost::mqtt5::qos_e::at_least_once>(
                    "boost-mqtt5/test", "Hello World!", boost::mqtt5::retain_e::no,
                    boost::mqtt5::publish_props {},
                    // You may bind the strand to this handler, but it is not necessary
                    // as the strand is already the default associated handler.
                    [&client, &strand](
                        boost::mqtt5::error_code ec, boost::mqtt5::reason_code rc,
                        boost::mqtt5::puback_props props
                    ) {
                        assert(strand.running_in_this_thread());

                        std::cout << ec.message() << std::endl;
                        std::cout << rc.message() << std::endl;

                        // Stop the Client.
                        client.cancel();
                    }
                );
            }
        )
    );

    tp.join();

    return 0;
}

//]
