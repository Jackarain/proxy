//
// Copyright (c) 2023-2025 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/use_awaitable.hpp>
#ifdef BOOST_ASIO_HAS_CO_AWAIT

//[hello_world_in_coro_multithreaded_env
#include <boost/mqtt5/logger.hpp>
#include <boost/mqtt5/mqtt_client.hpp>
#include <boost/mqtt5/types.hpp>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/detached.hpp>
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

// client_type with logging enabled
using client_type = boost::mqtt5::mqtt_client<
    boost::asio::ip::tcp::socket, std::monostate /* TlsContext */, boost::mqtt5::logger
>;

// client_type without logging
//using client_type = boost::mqtt5::mqtt_client<boost::asio::ip::tcp::socket>;

// Modified completion token that will prevent co_await from throwing exceptions.
constexpr auto use_nothrow_awaitable = boost::asio::as_tuple(boost::asio::deferred);

boost::asio::awaitable<void> publish_hello_world(
    const config& cfg, client_type& client,
    const boost::asio::strand<boost::asio::thread_pool::executor_type>& strand
) {
    // Confirmation that the coroutine running in the strand.
    assert(strand.running_in_this_thread());

    // All these function calls will be executed by the strand that is executing the coroutine.
    // All the completion handler's associated executors will be that same strand
    // because the Client was constructed with it as the default associated executor.
    client.brokers(cfg.brokers, cfg.port) // Set the Broker to connect to.
        .credentials(cfg.client_id) // Set the Client Identifier. (optional)
        .async_run(boost::asio::detached); // Start the Client.

    auto&& [ec, rc, puback_props] = co_await client.async_publish<boost::mqtt5::qos_e::at_least_once>(
        "boost-mqtt5/test" /* topic */, "Hello world!" /* payload*/, boost::mqtt5::retain_e::no,
        boost::mqtt5::publish_props {}, use_nothrow_awaitable);

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

    // Create a thread pool with 4 threads.
    boost::asio::thread_pool tp(4);

    // Create an explicit strand from io_context's executor.
    // The strand guarantees a serialised handler execution regardless of the 
    // number of threads running in the io_context.
    boost::asio::strand strand = boost::asio::make_strand(tp.get_executor());

    // Create the Client with the explicit strand as the default associated executor.
    client_type client(strand, {} /* tls_context */, boost::mqtt5::logger(boost::mqtt5::log_level::info));

    // Spawn the coroutine.
    // The executor that executes the coroutine must be the same executor
    // that is the Client's default associated executor.
    co_spawn(
        strand,
        publish_hello_world(cfg, client, strand),
        [](std::exception_ptr e) {
            if (e)
                std::rethrow_exception(e);
        }
    );

    tp.join();

    return 0;
}

//]

#else

#include <iostream>

int main() {
    std::cout << "This example requires C++20 standard to compile and run" << std::endl;
}

#endif
