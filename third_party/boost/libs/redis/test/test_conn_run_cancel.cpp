//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/connection.hpp>
#include <boost/redis/ignore.hpp>

#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/core/lightweight_test.hpp>

#include "common.hpp"

#include <cstddef>
#include <iostream>
#include <string_view>

using boost::system::error_code;
namespace net = boost::asio;
using namespace boost::redis;

namespace {

// Terminal and partial cancellation work for async_run
template <class Connection>
void test_per_operation_cancellation(std::string_view name, net::cancellation_type_t cancel_type)
{
   std::cerr << "Running test case: " << name << std::endl;

   // Setup
   net::io_context ioc;
   Connection conn{ioc};
   net::cancellation_signal sig;

   request req;
   req.push("PING", "something");

   bool run_finished = false, exec_finished = false;

   // Run the connection
   auto run_cb = [&](error_code ec) {
      run_finished = true;
      BOOST_TEST_EQ(ec, net::error::operation_aborted);
   };
   conn.async_run(make_test_config(), net::bind_cancellation_slot(sig.slot(), run_cb));

   // Launch a PING
   conn.async_exec(req, ignore, [&](error_code ec, std::size_t) {
      exec_finished = true;
      BOOST_TEST_EQ(ec, error_code());
      sig.emit(cancel_type);
   });

   ioc.run_for(test_timeout);

   // Check
   BOOST_TEST(run_finished);
   BOOST_TEST(exec_finished);
}

}  // namespace

int main()
{
   using basic_connection_t = basic_connection<net::io_context::executor_type>;

   test_per_operation_cancellation<basic_connection_t>(
      "basic_connection, terminal",
      net::cancellation_type_t::terminal);
   test_per_operation_cancellation<basic_connection_t>(
      "basic_connection, partial",
      net::cancellation_type_t::partial);

   test_per_operation_cancellation<connection>(
      "connection, terminal",
      net::cancellation_type_t::terminal);
   test_per_operation_cancellation<connection>(
      "connection, partial",
      net::cancellation_type_t::partial);

   return boost::report_errors();
}
