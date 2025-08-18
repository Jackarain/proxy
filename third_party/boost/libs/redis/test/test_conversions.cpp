/* Copyright (c) 2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/connection.hpp>
#include <boost/redis/ignore.hpp>

#include <boost/system/error_code.hpp>

#define BOOST_TEST_MODULE conversions
#include <boost/test/included/unit_test.hpp>

#include "common.hpp"

namespace net = boost::asio;
using boost::redis::connection;
using boost::redis::ignore_t;
using boost::redis::request;
using boost::redis::response;
using boost::system::error_code;

namespace {

BOOST_AUTO_TEST_CASE(ints)
{
   // Setup
   net::io_context ioc;
   auto conn = std::make_shared<connection>(ioc);
   run(conn);

   // Get an integer key as all possible C++ integral types
   request req;
   req.push("SET", "key", 42);
   for (int i = 0; i < 10; ++i)
      req.push("GET", "key");

   response<
      ignore_t,
      signed char,
      unsigned char,
      short,
      unsigned short,
      int,
      unsigned int,
      long,
      unsigned long,
      long long,
      unsigned long long>
      resp;

   bool finished = false;

   conn->async_exec(req, resp, [conn, &finished](error_code ec, std::size_t) {
      finished = true;
      BOOST_TEST(ec == error_code());
      conn->cancel();
   });

   // Run the operations
   ioc.run_for(test_timeout);
   BOOST_TEST(finished);

   // Check
   BOOST_TEST(std::get<1>(resp).value() == 42);
   BOOST_TEST(std::get<2>(resp).value() == 42u);
   BOOST_TEST(std::get<3>(resp).value() == 42);
   BOOST_TEST(std::get<4>(resp).value() == 42u);
   BOOST_TEST(std::get<5>(resp).value() == 42);
   BOOST_TEST(std::get<6>(resp).value() == 42u);
   BOOST_TEST(std::get<7>(resp).value() == 42);
   BOOST_TEST(std::get<8>(resp).value() == 42u);
   BOOST_TEST(std::get<9>(resp).value() == 42);
   BOOST_TEST(std::get<10>(resp).value() == 42u);
}

BOOST_AUTO_TEST_CASE(bools)
{
   // Setup
   net::io_context ioc;
   auto conn = std::make_shared<connection>(ioc);
   run(conn);

   // Get a boolean
   request req;
   req.push("SET", "key_true", "t");
   req.push("SET", "key_false", "f");
   req.push("GET", "key_true");
   req.push("GET", "key_false");

   response<ignore_t, ignore_t, bool, bool> resp;

   bool finished = false;

   conn->async_exec(req, resp, [conn, &finished](error_code ec, std::size_t) {
      finished = true;
      BOOST_TEST(ec == error_code());
      conn->cancel();
   });

   // Run the operations
   ioc.run_for(test_timeout);

   // Check
   BOOST_TEST(std::get<2>(resp).value() == true);
   BOOST_TEST(std::get<3>(resp).value() == false);
}

BOOST_AUTO_TEST_CASE(floating_points)
{
   // Setup
   net::io_context ioc;
   auto conn = std::make_shared<connection>(ioc);
   run(conn);

   // Get a boolean
   request req;
   req.push("SET", "key", "4.12");
   req.push("GET", "key");

   response<ignore_t, double> resp;

   bool finished = false;

   conn->async_exec(req, resp, [conn, &finished](error_code ec, std::size_t) {
      finished = true;
      BOOST_TEST(ec == error_code());
      conn->cancel();
   });

   // Run the operations
   ioc.run_for(test_timeout);
   BOOST_TEST(finished);

   // Check
   BOOST_TEST(std::get<1>(resp).value() == 4.12);
}

}  // namespace