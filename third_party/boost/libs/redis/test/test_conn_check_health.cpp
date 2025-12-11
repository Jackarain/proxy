/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/connection.hpp>
#include <boost/redis/ignore.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/response.hpp>

#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/core/lightweight_test.hpp>

#include "common.hpp"

#include <chrono>
#include <cstddef>
#include <string>

namespace net = boost::asio;
namespace redis = boost::redis;
using error_code = boost::system::error_code;
using connection = boost::redis::connection;
using boost::redis::request;
using boost::redis::ignore;
using boost::redis::generic_response;
using namespace std::chrono_literals;

namespace {

// The health checker detects dead connections and triggers reconnection
void test_reconnection()
{
   // Setup
   net::io_context ioc;
   connection conn{ioc};

   // This request will block forever, causing the connection to become unresponsive
   request req1;
   req1.push("BLPOP", "any", 0);

   // This request should be executed after reconnection
   request req2;
   req2.push("PING", "after_reconnection");
   req2.get_config().cancel_if_unresponded = false;
   req2.get_config().cancel_on_connection_lost = false;

   // Make the test run faster
   auto cfg = make_test_config();
   cfg.health_check_interval = 500ms;
   cfg.reconnect_wait_interval = 100ms;

   bool run_finished = false, exec1_finished = false, exec2_finished = false;

   conn.async_run(cfg, [&](error_code ec) {
      run_finished = true;
      BOOST_TEST_EQ(ec, net::error::operation_aborted);
   });

   // This request will complete after the health checker deems the connection
   // as unresponsive and triggers a reconnection (it's configured to be cancelled
   // on connection lost).
   conn.async_exec(req1, ignore, [&](error_code ec, std::size_t) {
      exec1_finished = true;
      BOOST_TEST_EQ(ec, net::error::operation_aborted);

      // Execute the second request. This one will succeed after reconnection
      conn.async_exec(req2, ignore, [&](error_code ec2, std::size_t) {
         exec2_finished = true;
         BOOST_TEST_EQ(ec2, error_code());
         conn.cancel();
      });
   });

   ioc.run_for(test_timeout);

   BOOST_TEST(run_finished);
   BOOST_TEST(exec1_finished);
   BOOST_TEST(exec2_finished);
}

// We use the correct error code when a ping times out
void test_error_code()
{
   // Setup
   net::io_context ioc;
   connection conn{ioc};

   // This request will block forever, causing the connection to become unresponsive
   request req;
   req.push("BLPOP", "any", 0);

   // Make the test run faster
   auto cfg = make_test_config();
   cfg.health_check_interval = 200ms;
   cfg.reconnect_wait_interval = 0s;

   bool run_finished = false, exec_finished = false;

   conn.async_run(cfg, [&](error_code ec) {
      run_finished = true;
      BOOST_TEST_EQ(ec, boost::redis::error::pong_timeout);
   });

   // This request will complete after the health checker deems the connection
   // as unresponsive and triggers a reconnection (it's configured to be cancelled
   // if unresponded).
   conn.async_exec(req, ignore, [&](error_code ec, std::size_t) {
      exec_finished = true;
      BOOST_TEST_EQ(ec, net::error::operation_aborted);
   });

   ioc.run_for(test_timeout);

   BOOST_TEST(run_finished);
   BOOST_TEST(exec_finished);
}

// A ping interval of zero disables timeouts (and doesn't cause trouble)
void test_disabled()
{
   // Setup
   net::io_context ioc;
   connection conn{ioc};

   // Run a couple of requests to verify that the connection works fine
   request req1;
   req1.push("PING", "health_check_disabled_1");

   request req2;
   req1.push("PING", "health_check_disabled_2");

   auto cfg = make_test_config();
   cfg.health_check_interval = 0s;

   bool run_finished = false, exec1_finished = false, exec2_finished = false;

   conn.async_run(cfg, [&](error_code ec) {
      run_finished = true;
      BOOST_TEST_EQ(ec, net::error::operation_aborted);
   });

   conn.async_exec(req1, ignore, [&](error_code ec, std::size_t) {
      exec1_finished = true;
      BOOST_TEST_EQ(ec, error_code());
      conn.async_exec(req2, ignore, [&](error_code ec2, std::size_t) {
         exec2_finished = true;
         BOOST_TEST_EQ(ec2, error_code());
         conn.cancel();
      });
   });

   ioc.run_for(test_timeout);

   BOOST_TEST(run_finished);
   BOOST_TEST(exec1_finished);
   BOOST_TEST(exec2_finished);
}

// Receiving data is sufficient to consider our connection healthy.
// Sends a blocking request that causes PINGs to not be answered,
// and subscribes to a channel to receive pushes periodically.
// This simulates situations of heavy load, where PINGs may not be answered on time.
class test_flexible {
   net::io_context ioc;
   connection conn1{ioc};  // The one that simulates a heavy load condition
   connection conn2{ioc};  // Publishes messages
   net::steady_timer timer{ioc};
   request publish_req;
   bool run1_finished = false, run2_finished = false, exec_finished{false},
        publisher_finished{false};

   // Starts publishing messages to the channel
   void start_publish()
   {
      conn2.async_exec(publish_req, ignore, [this](error_code ec, std::size_t) {
         BOOST_TEST_EQ(ec, error_code());

         if (exec_finished) {
            // The blocking request finished, we're done
            conn2.cancel();
            publisher_finished = true;
         } else {
            // Wait for some time and publish again
            timer.expires_after(100ms);
            timer.async_wait([this](error_code ec) {
               BOOST_TEST_EQ(ec, error_code());
               start_publish();
            });
         }
      });
   }

   // Generates a sufficiently unique name for channels so
   // tests may be run in parallel for different configurations
   static std::string make_unique_id()
   {
      auto t = std::chrono::high_resolution_clock::now();
      return "test-flexible-health-checks-" + std::to_string(t.time_since_epoch().count());
   }

public:
   test_flexible() = default;

   void run()
   {
      // Setup
      auto cfg = make_test_config();
      cfg.health_check_interval = 500ms;
      generic_response resp;

      std::string channel_name = make_unique_id();
      publish_req.push("PUBLISH", channel_name, "test_health_check_flexible");

      // This request will block for much longer than the health check
      // interval. If we weren't receiving pushes, the connection would be considered dead.
      // If this request finishes successfully, the health checker is doing good
      request blocking_req;
      blocking_req.push("SUBSCRIBE", channel_name);
      blocking_req.push("BLPOP", "any", 2);
      blocking_req.get_config().cancel_if_unresponded = true;
      blocking_req.get_config().cancel_on_connection_lost = true;

      conn1.async_run(cfg, [&](error_code ec) {
         run1_finished = true;
         BOOST_TEST_EQ(ec, net::error::operation_aborted);
      });

      conn2.async_run(cfg, [&](error_code ec) {
         run2_finished = true;
         BOOST_TEST_EQ(ec, net::error::operation_aborted);
      });

      // BLPOP will return NIL, so we can't use ignore
      conn1.async_exec(blocking_req, resp, [&](error_code ec, std::size_t) {
         exec_finished = true;
         BOOST_TEST_EQ(ec, error_code());
         conn1.cancel();
      });

      start_publish();

      ioc.run_for(test_timeout);

      BOOST_TEST(run1_finished);
      BOOST_TEST(run2_finished);
      BOOST_TEST(exec_finished);
      BOOST_TEST(publisher_finished);
   }
};

}  // namespace

int main()
{
   test_reconnection();
   test_error_code();
   test_disabled();
   test_flexible().run();

   return boost::report_errors();
}