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
#include <boost/core/lightweight_test.hpp>

#include "common.hpp"

#include <cstddef>

namespace net = boost::asio;
using boost::system::error_code;
using boost::redis::connection;
using boost::redis::request;
using boost::redis::ignore;
using boost::redis::operation;
using boost::redis::generic_response;
using boost::redis::consume_one;
using namespace std::chrono_literals;

namespace {

// Verifies that using the MONITOR command works properly.
// Opens a connection, issues a MONITOR, issues some commands to
// generate some traffic, and waits for several MONITOR messages to arrive.
class test_monitor {
   net::io_context ioc;
   connection conn{ioc};
   generic_response monitor_resp;
   request ping_req;
   bool run_finished = false, exec_finished = false, receive_finished = false;
   int num_pushes_received = 0;

   void start_receive()
   {
      conn.async_receive([this](error_code ec, std::size_t) {
         // We should expect one push entry, at least
         BOOST_TEST_EQ(ec, error_code());
         BOOST_TEST(monitor_resp.has_value());
         BOOST_TEST_NOT(monitor_resp.value().empty());

         // Log the value and consume it
         std::clog << "Event> " << monitor_resp.value().front().value << std::endl;
         consume_one(monitor_resp);

         if (++num_pushes_received >= 5) {
            receive_finished = true;
         } else {
            start_receive();
         }
      });
   }

   // Starts generating traffic so our receiver task can progress
   void start_generating_traffic()
   {
      conn.async_exec(ping_req, ignore, [this](error_code ec, std::size_t) {
         // PINGs should complete successfully
         BOOST_TEST_EQ(ec, error_code());

         // Once the receiver exits, stop sending requests and tear down the connection
         if (receive_finished) {
            conn.cancel();
            exec_finished = true;
         } else {
            start_generating_traffic();
         }
      });
   }

public:
   test_monitor() = default;

   void run()
   {
      // Setup
      ping_req.push("PING", "test_monitor");
      conn.set_receive_response(monitor_resp);

      request monitor_req;
      monitor_req.push("MONITOR");

      // Run the connection
      conn.async_run(make_test_config(), [&](error_code ec) {
         run_finished = true;
         BOOST_TEST_EQ(ec, net::error::operation_aborted);
      });

      // Issue the monitor, then start generating traffic
      conn.async_exec(monitor_req, ignore, [&](error_code ec, std::size_t) {
         BOOST_TEST_EQ(ec, error_code());
         start_generating_traffic();
      });

      // In parallel, start a subscriber
      start_receive();

      ioc.run_for(test_timeout);

      BOOST_TEST(run_finished);
      BOOST_TEST(receive_finished);
      BOOST_TEST(exec_finished);
   }
};

}  // namespace

int main()
{
   test_monitor{}.run();

   return boost::report_errors();
}