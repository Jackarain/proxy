/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/connection.hpp>
#include <boost/redis/push_parser.hpp>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/consign.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/signal_set.hpp>

#include <exception>
#include <iostream>
#include <unistd.h>

#if defined(BOOST_ASIO_HAS_CO_AWAIT)
#if defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR)

namespace asio = boost::asio;
using asio::posix::stream_descriptor;
using asio::signal_set;
using boost::asio::async_read_until;
using boost::asio::awaitable;
using boost::asio::co_spawn;
using boost::asio::consign;
using boost::asio::detached;
using boost::asio::dynamic_buffer;
using boost::redis::config;
using boost::redis::connection;
using boost::redis::generic_flat_response;
using boost::redis::request;
using boost::system::error_code;
using boost::redis::push_parser;
using boost::redis::push_view;
using namespace std::chrono_literals;

// Chat over Redis pubsub. To test, run this program from multiple
// terminals and type messages to stdin.

namespace {

auto rethrow_on_error = [](std::exception_ptr exc) {
   if (exc)
      std::rethrow_exception(exc);
};

auto receiver(std::shared_ptr<connection> conn) -> awaitable<void>
{
   // Set the receive response, so pushes are stored in resp
   generic_flat_response resp;
   conn->set_receive_response(resp);

   // Subscribe to the channel 'channel'. Using request::subscribe()
   // (instead of request::push()) makes the connection re-subscribe
   // to 'channel' whenever it re-connects to the server.
   request req;
   req.subscribe({"channel"});
   co_await conn->async_exec(req);

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

      std::cout << std::endl;

      resp.value().clear();
   }
}

// Publishes stdin messages to a Redis channel.
auto publisher(std::shared_ptr<stream_descriptor> in, std::shared_ptr<connection> conn)
   -> awaitable<void>
{
   for (std::string msg;;) {
      auto n = co_await async_read_until(*in, dynamic_buffer(msg, 1024), "\n");
      request req;
      req.push("PUBLISH", "channel", msg);
      co_await conn->async_exec(req);
      msg.erase(0, n);
   }
}

}  // namespace

// Called from the main function (see main.cpp)
auto co_main(config cfg) -> awaitable<void>
{
   auto ex = co_await asio::this_coro::executor;
   auto conn = std::make_shared<connection>(ex);
   auto stream = std::make_shared<stream_descriptor>(ex, ::dup(STDIN_FILENO));

   co_spawn(ex, receiver(conn), rethrow_on_error);
   co_spawn(ex, publisher(stream, conn), rethrow_on_error);
   conn->async_run(cfg, consign(detached, conn));

   signal_set sig_set{ex, SIGINT, SIGTERM};
   co_await sig_set.async_wait();
   conn->cancel();
   stream->cancel();
}

#else   // defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR)
auto co_main(config const&) -> awaitable<void>
{
   std::cout << "Requires support for posix streams." << std::endl;
   co_return;
}
#endif  // defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR)
#endif  // defined(BOOST_ASIO_HAS_CO_AWAIT)
