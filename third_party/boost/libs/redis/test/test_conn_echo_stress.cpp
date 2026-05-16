/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
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
using boost::redis::resp3::flat_tree;
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
      << "Bytes received (response): " << u.response_bytes_received << "\n"
      << "Bytes received (push): " << u.push_bytes_received << "\n"
      << "Bytes rotated: " << u.bytes_rotated;

   return os;
}

}  // namespace boost::redis

namespace {

auto receiver(connection& conn, flat_tree& resp, std::size_t expected) -> net::awaitable<void>
{
   std::size_t push_counter = 0;
   while (push_counter != expected) {
      co_await conn.async_receive2();
      push_counter += resp.get_total_msgs();
      resp.clear();
   }

   conn.cancel();
}

auto echo_session(connection& conn, const request& req, std::size_t n) -> net::awaitable<void>
{
   for (auto i = 0u; i < n; ++i)
      co_await conn.async_exec(req);
}

void rethrow_on_error(std::exception_ptr exc)
{
   if (exc) {
      BOOST_TEST(false);
      std::rethrow_exception(exc);
   }
}

request make_pub_req(std::size_t n_pubs)
{
   request req;
   req.push("PING");
   for (std::size_t i = 0u; i < n_pubs; ++i)
      req.push("PUBLISH", "channel", "payload");

   return req;
}

BOOST_AUTO_TEST_CASE(echo_stress)
{
   // Setup
   net::io_context ctx;
   connection conn{ctx};
   auto cfg = make_test_config();

   // Number of coroutines that will send pings sharing the same
   // connection to redis.
   constexpr std::size_t sessions = 150u;

   // The number of pings that will be sent by each session.
   constexpr std::size_t msgs = 200u;

   // The number of publishes that will be sent by each session with
   // each message.
   constexpr std::size_t n_pubs = 25u;

   // This is the total number of pushes we will receive.
   constexpr std::size_t total_pushes = sessions * msgs * n_pubs + 1;

   flat_tree resp;
   conn.set_receive_response(resp);

   request const pub_req = make_pub_req(n_pubs);

   // Run the connection
   bool run_finished = false, subscribe_finished = false;
   conn.async_run(cfg, logger{logger::level::crit}, [&run_finished](error_code ec) {
      run_finished = true;
      BOOST_TEST(ec == net::error::operation_aborted);
      std::clog << "async_run finished" << std::endl;
   });

   // Op that will consume the pushes counting down until all expected
   // pushes have been received.
   net::co_spawn(ctx, receiver(conn, resp, total_pushes), rethrow_on_error);

   // Subscribe, then launch the coroutines
   request req;
   req.subscribe({"channel"});
   conn.async_exec(req, ignore, [&](error_code ec, std::size_t) {
      subscribe_finished = true;
      BOOST_TEST(ec == error_code());

      for (std::size_t i = 0; i < sessions; ++i)
         net::co_spawn(ctx, echo_session(conn, pub_req, msgs), rethrow_on_error);
   });

   // Run the test
   ctx.run_for(2 * test_timeout);
   BOOST_TEST(run_finished);
   BOOST_TEST(subscribe_finished);

   // Print statistics
   std::cout << "-------------------\n"
             << "Usage data: \n"
             << conn.get_usage() << "\n"
             << "-------------------\n"
             << "Reallocations: " << resp.get_reallocs() << std::endl;
}

}  // namespace

#else
BOOST_AUTO_TEST_CASE(dummy) { }
#endif
