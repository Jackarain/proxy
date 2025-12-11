//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/connection.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/response.hpp>

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/core/lightweight_test.hpp>

#include "common.hpp"

#include <cstddef>
#include <string>

using boost::system::error_code;
namespace net = boost::asio;
using namespace boost::redis;

namespace {

// Move constructing a connection doesn't leave dangling pointers
void test_conn_move_construct()
{
   // Setup
   net::io_context ioc;
   connection conn_prev(ioc);
   connection conn(std::move(conn_prev));
   request req;
   req.push("PING", "something");
   response<std::string> res;

   bool run_finished = false, exec_finished = false;

   // Run the connection
   conn.async_run(make_test_config(), [&](error_code ec) {
      run_finished = true;
      BOOST_TEST_EQ(ec, net::error::operation_aborted);
   });

   // Launch a PING
   conn.async_exec(req, res, [&](error_code ec, std::size_t) {
      exec_finished = true;
      BOOST_TEST_EQ(ec, error_code());
      conn.cancel();
   });

   ioc.run_for(test_timeout);

   // Check
   BOOST_TEST(run_finished);
   BOOST_TEST(exec_finished);
   BOOST_TEST_EQ(std::get<0>(res).value(), "something");
}

// Moving a connection is safe even when it's running,
// and it doesn't leave dangling pointers
void test_conn_move_assign_while_running()
{
   // Setup
   net::io_context ioc;
   connection conn(ioc);
   connection conn2(ioc);  // will be assigned to
   request req;
   req.push("PING", "something");
   response<std::string> res;

   bool run_finished = false, exec_finished = false;

   // Run the connection
   conn.async_run(make_test_config(), [&](error_code ec) {
      run_finished = true;
      BOOST_TEST_EQ(ec, net::error::operation_aborted);
   });

   // Launch a PING. When it finishes, conn will be moved-from, and conn2 will be valid
   conn.async_exec(req, res, [&](error_code ec, std::size_t) {
      exec_finished = true;
      BOOST_TEST_EQ(ec, error_code());
      conn2.cancel();
   });

   // While the operations are running, perform a move
   net::post(net::bind_executor(ioc.get_executor(), [&] {
      conn2 = std::move(conn);
   }));

   ioc.run_for(test_timeout);

   // Check
   BOOST_TEST(run_finished);
   BOOST_TEST(exec_finished);
   BOOST_TEST_EQ(std::get<0>(res).value(), "something");
}

}  // namespace

int main()
{
   test_conn_move_construct();
   test_conn_move_assign_while_running();

   return boost::report_errors();
}
