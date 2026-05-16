/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/connection.hpp>
#include <boost/redis/push_parser.hpp>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/consign.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/signal_set.hpp>

#include <iostream>

#if defined(BOOST_ASIO_HAS_CO_AWAIT)

namespace asio = boost::asio;
using namespace std::chrono_literals;
using boost::redis::request;
using boost::redis::generic_flat_response;
using boost::redis::config;
using boost::redis::connection;
using boost::redis::push_parser;
using boost::redis::push_view;
using boost::system::error_code;
using asio::signal_set;

/* This example will subscribe and read pushes indefinitely.
 *
 * To test send messages with redis-cli
 *
 *    $ redis-cli -3
 *    127.0.0.1:6379> PUBLISH mychannel some-message
 *    (integer) 3
 *    127.0.0.1:6379>
 *
 * To test reconnection try, for example, to close all clients currently
 * connected to the Redis instance
 *
 * $ redis-cli
 * > CLIENT kill TYPE pubsub
 */

// Receives server pushes.
auto receiver(std::shared_ptr<connection> conn) -> asio::awaitable<void>
{
   generic_flat_response resp;
   conn->set_receive_response(resp);

   // Subscribe to the channel 'mychannel'. You can add any number of channels here.
   request req;
   req.subscribe({"mychannel"});
   co_await conn->async_exec(req);

   // You're now subscribed to 'mychannel'. Pushes sent over this channel will be stored
   // in resp. If the connection encounters a network error and reconnects to the server,
   // it will automatically subscribe to 'mychannel' again. This is transparent to the user.
   // You need to use specialized request::subscribe() function (instead of request::push)
   // to enable this behavior.

   // Loop to read Redis push messages.
   while (conn->will_reconnect()) {
      // Wait for pushes
      auto [ec] = co_await conn->async_receive2(asio::as_tuple);

      // Check for errors and cancellations
      if (ec) {
         std::cerr << "Error during receive: " << ec << std::endl;
         break;
      }

      // This can happen if a SUBSCRIBE command errored (e.g. insufficient permissions)
      if (resp.has_error()) {
         std::cerr << "The receive response contains an error: " << resp.error().diagnostic
                   << std::endl;
         break;
      }

      // The response must be consumed without suspending the
      // coroutine i.e. without the use of async operations.
      for (push_view elem : push_parser(resp.value())) {
         std::cout << "Received message from channel " << elem.channel << ": " << elem.payload
                   << "\n";
      }

      resp.value().clear();
   }
}

auto co_main(config cfg) -> asio::awaitable<void>
{
   auto ex = co_await asio::this_coro::executor;
   auto conn = std::make_shared<connection>(ex);
   asio::co_spawn(ex, receiver(conn), asio::detached);
   conn->async_run(cfg, asio::consign(asio::detached, conn));

   signal_set sig_set(ex, SIGINT, SIGTERM);
   co_await sig_set.async_wait();

   conn->cancel();
}

#endif  // defined(BOOST_ASIO_HAS_CO_AWAIT)
