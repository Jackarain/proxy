/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/connection.hpp>
#include <boost/redis/logger.hpp>

#include <boost/asio/error.hpp>
#include <boost/system/error_code.hpp>

#define BOOST_TEST_MODULE issue_181
#include <boost/test/included/unit_test.hpp>

#include "common.hpp"

#include <chrono>
#include <iostream>

namespace net = boost::asio;
using boost::redis::request;
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

BOOST_AUTO_TEST_CASE(issue_181)
{
   using basic_connection = boost::redis::basic_connection<net::any_io_executor>;

   auto const level = boost::redis::logger::level::debug;
   net::io_context ioc;
   auto ctx = net::ssl::context{net::ssl::context::tlsv12_client};
   basic_connection conn{ioc.get_executor(), std::move(ctx)};
   net::steady_timer timer{ioc};
   timer.expires_after(std::chrono::seconds{1});

   bool run_finished = false;

   auto run_cont = [&](error_code ec) {
      std::cout << "async_run1: " << ec.message() << std::endl;
      BOOST_TEST(ec == net::error::operation_aborted);
      run_finished = true;
   };

   auto cfg = make_test_config();
   cfg.health_check_interval = std::chrono::seconds{0};
   cfg.reconnect_wait_interval = std::chrono::seconds{0};
   conn.async_run(cfg, boost::redis::logger{level}, run_cont);
   BOOST_TEST(!conn.run_is_canceled());

   // Uses a timer to wait some time until run has been called.
   auto timer_cont = [&](error_code ec) {
      std::cout << "timer_cont: " << ec.message() << std::endl;
      BOOST_TEST(ec == error_code());
      BOOST_TEST(!conn.run_is_canceled());
      conn.cancel(operation::run);
      BOOST_TEST(conn.run_is_canceled());
   };

   timer.async_wait(timer_cont);

   ioc.run_for(test_timeout);

   BOOST_TEST(run_finished);
}

}  // namespace