/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/connection.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/system/error_code.hpp>

#include "common.hpp"

#include <cstddef>
#include <iostream>

namespace net = boost::asio;
using boost::redis::connection;
using boost::system::error_code;
using boost::redis::operation;
using boost::redis::request;
using boost::redis::response;
using boost::redis::ignore;
using namespace std::chrono_literals;

// Test if quit causes async_run to exit.
void test_async_run_exits()
{
   net::io_context ioc;

   auto conn = std::make_shared<connection>(ioc);

   request req1;
   req1.get_config().cancel_on_connection_lost = false;
   req1.push("PING");

   request req2;
   req2.get_config().cancel_on_connection_lost = false;
   req2.push("QUIT");

   // Should fail since this request will be sent after quit.
   request req3;
   req3.get_config().cancel_if_not_connected = true;
   req3.get_config().cancel_on_connection_lost = true;
   req3.push("PING");

   bool c1_called = false, c2_called = false, c3_called = false;

   auto c3 = [&](error_code ec, std::size_t) {
      c3_called = true;
      std::clog << "c3: " << ec.message() << std::endl;
      BOOST_TEST_EQ(ec, net::error::operation_aborted);
   };

   auto c2 = [&](error_code ec, std::size_t) {
      c2_called = true;
      std::clog << "c2: " << ec.message() << std::endl;
      BOOST_TEST_EQ(ec, error_code());
      conn->async_exec(req3, ignore, c3);
   };

   auto c1 = [&](error_code ec, std::size_t) {
      c1_called = true;
      std::cout << "c1: " << ec.message() << std::endl;
      BOOST_TEST_EQ(ec, error_code());
      conn->async_exec(req2, ignore, c2);
   };

   conn->async_exec(req1, ignore, c1);

   // The healthy checker should not be the cause of async_run
   // completing, so we disable.
   auto cfg = make_test_config();
   cfg.health_check_interval = 0s;
   cfg.reconnect_wait_interval = 0s;
   run(conn, cfg);

   ioc.run_for(test_timeout);

   BOOST_TEST(c1_called);
   BOOST_TEST(c2_called);
   BOOST_TEST(c3_called);
}

int main()
{
   test_async_run_exits();

   return boost::report_errors();
}