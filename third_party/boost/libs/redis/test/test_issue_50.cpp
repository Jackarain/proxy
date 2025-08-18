/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

// Must come before any asio header, otherwise build fails on msvc.

#include <boost/redis/connection.hpp>
#include <boost/redis/logger.hpp>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/system/error_code.hpp>

#include <exception>
#define BOOST_TEST_MODULE issue50
#include <boost/test/included/unit_test.hpp>

#include "common.hpp"

#include <iostream>

#if defined(BOOST_ASIO_HAS_CO_AWAIT)

namespace net = boost::asio;
using boost::redis::request;
using boost::redis::response;
using boost::redis::ignore;
using boost::redis::logger;
using boost::redis::config;
using boost::redis::operation;
using boost::redis::connection;
using boost::system::error_code;
using namespace std::chrono_literals;

namespace {

// Push consumer
auto receiver(std::shared_ptr<connection> conn) -> net::awaitable<void>
{
   std::cout << "uuu" << std::endl;
   while (conn->will_reconnect()) {
      std::cout << "dddd" << std::endl;
      // Loop reading Redis pushs messages.
      for (;;) {
         std::cout << "aaaa" << std::endl;
         error_code ec;
         co_await conn->async_receive(net::redirect_error(ec));
         if (ec) {
            std::cout << "Error in async_receive" << std::endl;
            break;
         }
      }
   }

   std::cout << "Exiting the receiver." << std::endl;
}

auto periodic_task(std::shared_ptr<connection> conn) -> net::awaitable<void>
{
   net::steady_timer timer{co_await net::this_coro::executor};
   for (int i = 0; i < 10; ++i) {
      std::cout << "In the loop: " << i << std::endl;
      timer.expires_after(std::chrono::milliseconds(50));
      co_await timer.async_wait();

      // Key is not set so it will cause an error since we are passing
      // an adapter that does not accept null, this will cause an error
      // that result in the connection being closed.
      request req;
      req.push("GET", "mykey");
      auto [ec, u] = co_await conn->async_exec(req, ignore, net::as_tuple);
      if (ec) {
         std::cout << "(1)Error: " << ec << std::endl;
      } else {
         std::cout << "no error: " << std::endl;
      }
   }

   std::cout << "Periodic task done!" << std::endl;
   conn->cancel(operation::run);
   conn->cancel(operation::receive);
   conn->cancel(operation::reconnection);
}

BOOST_AUTO_TEST_CASE(issue_50)
{
   bool receiver_finished = false, periodic_finished = false, run_finished = false;

   net::io_context ctx;
   auto conn = std::make_shared<connection>(ctx.get_executor());

   // Launch the receiver
   net::co_spawn(ctx, receiver(conn), [&](std::exception_ptr exc) {
      if (exc)
         std::rethrow_exception(exc);
      receiver_finished = true;
   });

   // Launch the period task
   net::co_spawn(ctx, periodic_task(conn), [&](std::exception_ptr exc) {
      if (exc)
         std::rethrow_exception(exc);
      periodic_finished = true;
   });

   // Launch run
   conn->async_run(make_test_config(), {}, [&](error_code) {
      run_finished = true;
   });

   ctx.run_for(2 * test_timeout);

   BOOST_TEST(receiver_finished);
   BOOST_TEST(periodic_finished);
   BOOST_TEST(run_finished);
}

}  // namespace

#else
BOOST_AUTO_TEST_CASE(dummy) { }
#endif  // defined(BOOST_ASIO_HAS_CO_AWAIT)
