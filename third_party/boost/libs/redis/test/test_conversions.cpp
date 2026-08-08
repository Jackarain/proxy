/* Copyright (c) 2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/connection.hpp>
#include <boost/redis/ignore.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/system/error_code.hpp>

#include "common.hpp"

namespace net = boost::asio;
using boost::redis::connection;
using boost::redis::ignore_t;
using boost::redis::request;
using boost::redis::response;
using boost::system::error_code;

namespace {

void test_ints()
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
      BOOST_TEST_EQ(ec, error_code());
      conn->cancel();
   });

   // Run the operations
   ioc.run_for(test_timeout);
   BOOST_TEST(finished);

   // Check
   BOOST_TEST_EQ(std::get<1>(resp).value(), 42);
   BOOST_TEST_EQ(std::get<2>(resp).value(), 42u);
   BOOST_TEST_EQ(std::get<3>(resp).value(), 42);
   BOOST_TEST_EQ(std::get<4>(resp).value(), 42u);
   BOOST_TEST_EQ(std::get<5>(resp).value(), 42);
   BOOST_TEST_EQ(std::get<6>(resp).value(), 42u);
   BOOST_TEST_EQ(std::get<7>(resp).value(), 42);
   BOOST_TEST_EQ(std::get<8>(resp).value(), 42u);
   BOOST_TEST_EQ(std::get<9>(resp).value(), 42);
   BOOST_TEST_EQ(std::get<10>(resp).value(), 42u);
}

void test_bools()
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
      BOOST_TEST_EQ(ec, error_code());
      conn->cancel();
   });

   // Run the operations
   ioc.run_for(test_timeout);

   // Check
   BOOST_TEST(std::get<2>(resp).value());
   BOOST_TEST_NOT(std::get<3>(resp).value());
}

void test_floating_points()
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
      BOOST_TEST_EQ(ec, error_code());
      conn->cancel();
   });

   // Run the operations
   ioc.run_for(test_timeout);
   BOOST_TEST(finished);

   // Check
   BOOST_TEST_EQ(std::get<1>(resp).value(), 4.12);
}

}  // namespace

int main()
{
   test_ints();
   test_bools();
   test_floating_points();

   return boost::report_errors();
}