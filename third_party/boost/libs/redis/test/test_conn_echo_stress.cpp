/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/connection.hpp>
#include <boost/redis/logger.hpp>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>

#include <cstddef>
#include <exception>
#define BOOST_TEST_MODULE echo_stress
#include <boost/test/included/unit_test.hpp>

#include "common.hpp"

#include <iostream>

#ifdef BOOST_ASIO_HAS_CO_AWAIT

namespace net = boost::asio;
using error_code = boost::system::error_code;
using boost::redis::operation;
using boost::redis::request;
using boost::redis::response;
using boost::redis::ignore;
using boost::redis::ignore_t;
using boost::redis::logger;
using boost::redis::connection;
using boost::redis::usage;
using boost::redis::error;
using namespace std::chrono_literals;

namespace boost::redis {

std::ostream& operator<<(std::ostream& os, usage const& u)
{
   os << "Commands sent: " << u.commands_sent << "\n"
      << "Bytes sent: " << u.bytes_sent << "\n"
      << "Responses received: " << u.responses_received << "\n"
      << "Pushes received: " << u.pushes_received << "\n"
      << "Response bytes received: " << u.response_bytes_received << "\n"
      << "Push bytes received: " << u.push_bytes_received;

   return os;
}

}  // namespace boost::redis

namespace {

auto push_consumer(connection& conn, int expected) -> net::awaitable<void>
{
   int c = 0;
   for (error_code ec;;) {
      conn.receive(ec);
      if (ec == error::sync_receive_push_failed) {
         ec = {};
         co_await conn.async_receive(net::redirect_error(ec));
      } else if (!ec) {
         //std::cout << "Skipping suspension." << std::endl;
      }

      if (ec) {
         BOOST_TEST(false, "push_consumer error: " << ec.message());
         co_return;
      }
      if (++c == expected)
         break;
   }

   conn.cancel();
}

auto echo_session(connection& conn, const request& pubs, int n) -> net::awaitable<void>
{
   for (auto i = 0; i < n; ++i)
      co_await conn.async_exec(pubs);
}

void rethrow_on_error(std::exception_ptr exc)
{
   if (exc)
      std::rethrow_exception(exc);
}

BOOST_AUTO_TEST_CASE(echo_stress)
{
   // Setup
   net::io_context ctx;
   connection conn{ctx};
   auto cfg = make_test_config();
   cfg.health_check_interval = std::chrono::seconds::zero();

   // Number of coroutines that will send pings sharing the same
   // connection to redis.
   constexpr int sessions = 150;

   // The number of pings that will be sent by each session.
   constexpr int msgs = 200;

   // The number of publishes that will be sent by each session with
   // each message.
   constexpr int n_pubs = 25;

   // This is the total number of pushes we will receive.
   constexpr int total_pushes = sessions * msgs * n_pubs + 1;

   request pubs;
   pubs.push("PING");
   for (int i = 0; i < n_pubs; ++i)
      pubs.push("PUBLISH", "channel", "payload");

   // Run the connection
   bool run_finished = false, subscribe_finished = false;
   conn.async_run(cfg, logger{logger::level::crit}, [&run_finished](error_code ec) {
      run_finished = true;
      BOOST_TEST(ec == net::error::operation_aborted);
      std::clog << "async_run finished" << std::endl;
   });

   // Subscribe, then launch the coroutines
   request req;
   req.push("SUBSCRIBE", "channel");
   conn.async_exec(req, ignore, [&](error_code ec, std::size_t) {
      subscribe_finished = true;
      BOOST_TEST(ec == error_code());

      // Op that will consume the pushes counting down until all expected
      // pushes have been received.
      net::co_spawn(ctx, push_consumer(conn, total_pushes), rethrow_on_error);

      for (int i = 0; i < sessions; ++i)
         net::co_spawn(ctx, echo_session(conn, pubs, msgs), rethrow_on_error);
   });

   // Run the test
   ctx.run_for(2 * test_timeout);
   BOOST_TEST(run_finished);
   BOOST_TEST(subscribe_finished);

   // Print statistics
   std::cout << "-------------------\n" << conn.get_usage() << std::endl;
}

}  // namespace

#else
BOOST_AUTO_TEST_CASE(dummy) { }
#endif
