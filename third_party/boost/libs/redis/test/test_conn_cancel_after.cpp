//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/connection.hpp>
#include <boost/redis/ignore.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/response.hpp>

#include <boost/asio/cancel_after.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/experimental/channel_error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/core/lightweight_test.hpp>

#include "common.hpp"

using namespace std::chrono_literals;
namespace asio = boost::asio;
using boost::system::error_code;
using boost::redis::request;
using boost::redis::basic_connection;
using boost::redis::connection;
using boost::redis::ignore;
using boost::redis::generic_response;

namespace {

template <class Connection>
void test_run()
{
   // Setup
   asio::io_context ioc;
   Connection conn{ioc};
   bool run_finished = false;

   // Call the function with a very short timeout
   conn.async_run(make_test_config(), asio::cancel_after(1ms, [&](error_code ec) {
                     BOOST_TEST_EQ(ec, asio::error::operation_aborted);
                     run_finished = true;
                  }));

   ioc.run_for(test_timeout);

   BOOST_TEST(run_finished);
}

template <class Connection>
void test_exec()
{
   // Setup
   asio::io_context ioc;
   Connection conn{ioc};
   bool exec_finished = false;

   request req;
   req.push("PING", "cancel_after");

   // Call the function with a very short timeout.
   // The connection is not being run, so these can't succeed
   conn.async_exec(req, ignore, asio::cancel_after(1ms, [&](error_code ec, std::size_t) {
                      BOOST_TEST_EQ(ec, asio::error::operation_aborted);
                      exec_finished = true;
                   }));

   ioc.run_for(test_timeout);

   BOOST_TEST(exec_finished);
}

template <class Connection>
void test_receive()
{
   // Setup
   asio::io_context ioc;
   Connection conn{ioc};
   bool receive_finished = false;
   generic_response resp;
   conn.set_receive_response(resp);

   // Call the function with a very short timeout.
   conn.async_receive(asio::cancel_after(1ms, [&](error_code ec, std::size_t) {
      BOOST_TEST_EQ(ec, asio::experimental::channel_errc::channel_cancelled);
      receive_finished = true;
   }));

   ioc.run_for(test_timeout);

   BOOST_TEST(receive_finished);
}

}  // namespace

int main()
{
   test_run<basic_connection<asio::io_context::executor_type>>();
   test_run<connection>();

   test_exec<basic_connection<asio::io_context::executor_type>>();
   test_exec<connection>();

   test_receive<basic_connection<asio::io_context::executor_type>>();
   test_receive<connection>();

   return boost::report_errors();
}
