/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/adapter/any_adapter.hpp>
#include <boost/redis/connection.hpp>

#include <boost/asio/detached.hpp>

#include <cstddef>
#include <string>
#define BOOST_TEST_MODULE conn_exec
#include <boost/test/included/unit_test.hpp>

#include "common.hpp"

#include <iostream>

// TODO: Test whether HELLO won't be inserted past commands that have
// been already written.
// TODO: Test async_exec with empty request e.g. hgetall with an empty
// container.

namespace net = boost::asio;
using boost::redis::config;
using boost::redis::connection;
using boost::redis::generic_response;
using boost::redis::ignore;
using boost::redis::operation;
using boost::redis::request;
using boost::redis::response;
using boost::system::error_code;
using namespace std::chrono_literals;

namespace {

// Sends three requests where one of them has a hello with a priority
// set, which means it should be executed first.
BOOST_AUTO_TEST_CASE(hello_priority)
{
   request req1;
   req1.push("PING", "req1");

   request req2;
   req2.get_config().hello_with_priority = false;
   req2.push("HELLO", 3);
   req2.push("PING", "req2");

   request req3;
   req3.get_config().hello_with_priority = true;
   req3.push("HELLO", 3);
   req3.push("PING", "req3");

   net::io_context ioc;

   auto conn = std::make_shared<connection>(ioc);

   bool seen1 = false;
   bool seen2 = false;
   bool seen3 = false;

   conn->async_exec(req1, ignore, [&](error_code ec, std::size_t) {
      // Second callback to the called.
      std::cout << "req1" << std::endl;
      BOOST_TEST(ec == error_code());
      BOOST_TEST(!seen2);
      BOOST_TEST(seen3);
      seen1 = true;
   });

   conn->async_exec(req2, ignore, [&](error_code ec, std::size_t) {
      // Last callback to the called.
      std::cout << "req2" << std::endl;
      BOOST_TEST(ec == error_code());
      BOOST_TEST(seen1);
      BOOST_TEST(seen3);
      seen2 = true;
      conn->cancel(operation::run);
      conn->cancel(operation::reconnection);
   });

   conn->async_exec(req3, ignore, [&](error_code ec, std::size_t) {
      // Callback that will be called first.
      std::cout << "req3" << std::endl;
      BOOST_TEST(ec == error_code());
      BOOST_TEST(!seen1);
      BOOST_TEST(!seen2);
      seen3 = true;
   });

   run(conn);
   ioc.run_for(test_timeout);
   BOOST_TEST(seen1);
   BOOST_TEST(seen2);
   BOOST_TEST(seen3);
}

// Tries to receive a string in an int and gets an error.
BOOST_AUTO_TEST_CASE(wrong_response_data_type)
{
   request req;
   req.push("PING");

   // Wrong data type.
   response<int> resp;
   net::io_context ioc;

   auto conn = std::make_shared<connection>(ioc);
   bool finished = false;

   conn->async_exec(req, resp, [conn, &finished](error_code ec, std::size_t) {
      BOOST_TEST(ec == boost::redis::error::not_a_number);
      conn->cancel(operation::reconnection);
      finished = true;
   });

   run(conn);
   ioc.run_for(test_timeout);
   BOOST_TEST(finished);
}

BOOST_AUTO_TEST_CASE(cancel_request_if_not_connected)
{
   request req;
   req.get_config().cancel_if_not_connected = true;
   req.push("PING");

   net::io_context ioc;
   auto conn = std::make_shared<connection>(ioc);
   bool finished = false;
   conn->async_exec(req, ignore, [conn, &finished](error_code ec, std::size_t) {
      BOOST_TEST(ec, boost::redis::error::not_connected);
      conn->cancel();
      finished = true;
   });

   ioc.run_for(test_timeout);
   BOOST_TEST(finished);
}

BOOST_AUTO_TEST_CASE(correct_database)
{
   auto cfg = make_test_config();
   cfg.database_index = 2;

   net::io_context ioc;

   auto conn = std::make_shared<connection>(ioc);

   request req;
   req.push("CLIENT", "LIST");

   generic_response resp;

   bool exec_finished = false, run_finished = false;

   conn->async_exec(req, resp, [&](error_code ec, std::size_t n) {
      BOOST_TEST(ec == error_code());
      std::clog << "async_exec has completed: " << n << std::endl;
      conn->cancel();
      exec_finished = true;
   });

   conn->async_run(cfg, {}, [&run_finished](error_code) {
      std::clog << "async_run has exited." << std::endl;
      run_finished = true;
   });

   ioc.run_for(test_timeout);
   BOOST_TEST_REQUIRE(exec_finished);
   BOOST_TEST_REQUIRE(run_finished);

   BOOST_TEST_REQUIRE(!resp.value().empty());
   auto const& value = resp.value().front().value;
   auto const pos = value.find("db=");
   auto const index_str = value.substr(pos + 3, 1);
   auto const index = std::stoi(index_str);

   // This check might fail if more than one client is connected to
   // redis when the CLIENT LIST command is run.
   BOOST_CHECK_EQUAL(cfg.database_index.value(), index);
}

BOOST_AUTO_TEST_CASE(large_number_of_concurrent_requests_issue_170)
{
   // See https://github.com/boostorg/redis/issues/170

   std::string payload;
   payload.resize(1024);
   std::fill(std::begin(payload), std::end(payload), 'A');

   net::io_context ioc;
   auto conn = std::make_shared<connection>(ioc);

   auto cfg = make_test_config();
   cfg.health_check_interval = std::chrono::seconds(0);
   conn->async_run(cfg, {}, net::detached);

   constexpr int repeat = 8000;
   int remaining = repeat;

   for (int i = 0; i < repeat; ++i) {
      auto req = std::make_shared<request>();
      req->push("PING", payload);
      conn->async_exec(*req, ignore, [req, &remaining, conn](error_code ec, std::size_t) {
         BOOST_TEST(ec == error_code());
         if (--remaining == 0)
            conn->cancel();
      });
   }

   ioc.run_for(test_timeout);

   BOOST_TEST(remaining == 0);
}

BOOST_AUTO_TEST_CASE(exec_any_adapter)
{
   // Executing an any_adapter object works
   request req;
   req.push("PING", "PONG");
   response<std::string> res;

   net::io_context ioc;

   auto conn = std::make_shared<connection>(ioc);

   bool finished = false;

   conn->async_exec(req, boost::redis::any_adapter(res), [&](error_code ec, std::size_t) {
      BOOST_TEST(ec == error_code());
      conn->cancel();
      finished = true;
   });

   run(conn);
   ioc.run_for(test_timeout);
   BOOST_TEST_REQUIRE(finished);

   BOOST_TEST(std::get<0>(res).value() == "PONG");
}

}  // namespace