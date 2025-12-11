/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/connection.hpp>
#include <boost/redis/ignore.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/response.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/cancel_after.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/system/errc.hpp>

#include "common.hpp"

#include <cstddef>
#include <iostream>

using namespace std::chrono_literals;

namespace net = boost::asio;
using error_code = boost::system::error_code;
using boost::redis::operation;
using boost::redis::error;
using boost::redis::request;
using boost::redis::response;
using boost::redis::generic_response;
using boost::redis::ignore;
using boost::redis::ignore_t;
using boost::redis::logger;
using boost::redis::connection;
using namespace std::chrono_literals;

namespace {

// We can cancel requests that haven't been written yet.
// All cancellation types are supported here.
void test_cancel_pending()
{
   struct {
      const char* name;
      net::cancellation_type_t cancel_type;
   } test_cases[] = {
      {"terminal", net::cancellation_type_t::terminal},
      {"partial",  net::cancellation_type_t::partial },
      {"total",    net::cancellation_type_t::total   },
   };

   for (const auto& tc : test_cases) {
      std::cerr << "Running test case: " << tc.name << std::endl;

      // Setup
      net::io_context ctx;
      connection conn(ctx);
      request req;
      req.push("get", "mykey");

      // Issue a request without calling async_run(), so the request stays waiting forever
      net::cancellation_signal sig;
      bool called = false;
      conn.async_exec(
         req,
         ignore,
         net::bind_cancellation_slot(sig.slot(), [&](error_code ec, std::size_t sz) {
            BOOST_TEST_EQ(ec, net::error::operation_aborted);
            BOOST_TEST_EQ(sz, 0u);
            called = true;
         }));

      // Issue a cancellation
      sig.emit(tc.cancel_type);

      // Prevent the test for deadlocking in case of failure
      ctx.run_for(test_timeout);
      BOOST_TEST(called);
   }
}

// We can cancel requests that have been written but which
// responses haven't been received yet.
// Terminal and partial cancellation types are supported here.
void test_cancel_written()
{
   // Setup
   net::io_context ctx;
   connection conn{ctx};
   auto cfg = make_test_config();
   cfg.health_check_interval = std::chrono::seconds::zero();
   bool run_finished = false, exec1_finished = false, exec2_finished = false,
        exec3_finished = false;

   // Will be cancelled after it has been written but before the
   // response arrives. Create everything in dynamic memory to verify
   // we don't try to access things after completion.
   auto req1 = std::make_unique<request>();
   req1->push("BLPOP", "any", 1);
   auto r1 = std::make_unique<response<std::string>>();

   // Will be cancelled too because it's sent after BLPOP.
   // Tests that partial cancellation is supported, too.
   request req2;
   req2.push("PING", "partial_cancellation");

   // Will finish successfully once the response to the BLPOP arrives
   request req3;
   req3.push("PING", "after_blpop");
   response<std::string> r3;

   // Run the connection
   conn.async_run(cfg, [&](error_code ec) {
      BOOST_TEST_EQ(ec, net::error::operation_aborted);
      run_finished = true;
   });

   // The request will be cancelled before it receives a response.
   // Our BLPOP will wait for longer than the timeout we're using.
   // Clear allocated memory to check we don't access the request or
   // response when the server response arrives.
   auto blpop_cb = [&](error_code ec, std::size_t) {
      req1.reset();
      r1.reset();
      BOOST_TEST_EQ(ec, net::error::operation_aborted);
      exec1_finished = true;
   };
   conn.async_exec(*req1, *r1, net::cancel_after(500ms, blpop_cb));

   // The first PING will be cancelled, too. Use partial cancellation here.
   auto req2_cb = [&](error_code ec, std::size_t) {
      BOOST_TEST_EQ(ec, net::error::operation_aborted);
      exec2_finished = true;
   };
   conn.async_exec(
      req2,
      ignore,
      net::cancel_after(500ms, net::cancellation_type_t::partial, req2_cb));

   // The second PING's response will be received after the BLPOP's response,
   // but it will be processed successfully.
   conn.async_exec(req3, r3, [&](error_code ec, std::size_t) {
      BOOST_TEST_EQ(ec, error_code());
      BOOST_TEST_EQ(std::get<0>(r3).value(), "after_blpop");
      conn.cancel();
      exec3_finished = true;
   });

   ctx.run_for(test_timeout);
   BOOST_TEST(run_finished);
   BOOST_TEST(exec1_finished);
   BOOST_TEST(exec2_finished);
   BOOST_TEST(exec3_finished);
}

// Requests configured to do so are cancelled if the connection
// hasn't been established when they are executed
void test_cancel_if_not_connected()
{
   net::io_context ioc;
   connection conn{ioc};

   request req;
   req.get_config().cancel_if_not_connected = true;
   req.push("PING");

   bool exec_finished = false;
   conn.async_exec(req, ignore, [&](error_code ec, std::size_t) {
      BOOST_TEST_EQ(ec, error::not_connected);
      exec_finished = true;
   });

   ioc.run_for(test_timeout);
   BOOST_TEST(exec_finished);
}

// Requests configured to do so are cancelled when the connection is lost.
// Tests with a written request that hasn't been responded yet
void test_cancel_on_connection_lost_written()
{
   // Setup
   net::io_context ioc;
   connection conn{ioc};

   // req0 and req1 will be coalesced together. When req0
   // completes, we know that req1 will be waiting for a response.
   // req1 will block forever.
   request req0;
   req0.push("PING");

   request req1;
   req1.get_config().cancel_on_connection_lost = true;
   req1.get_config().cancel_if_unresponded = true;
   req1.push("BLPOP", "any", 0);

   bool run_finished = false, exec0_finished = false, exec1_finished = false;

   // Run the connection
   auto cfg = make_test_config();
   conn.async_run(cfg, [&](error_code ec) {
      BOOST_TEST_EQ(ec, net::error::operation_aborted);
      run_finished = true;
   });

   // Execute both requests
   conn.async_exec(req0, ignore, [&](error_code ec, std::size_t) {
      // The request finished successfully
      BOOST_TEST_EQ(ec, error_code());
      exec0_finished = true;

      // We know that req1 has been written to the server, too. Trigger a cancellation
      conn.cancel(operation::run);
      conn.cancel(operation::reconnection);
   });

   conn.async_exec(req1, ignore, [&](error_code ec, std::size_t) {
      BOOST_TEST_EQ(ec, net::error::operation_aborted);
      exec1_finished = true;
   });

   ioc.run_for(test_timeout);
   BOOST_TEST(run_finished);
   BOOST_TEST(exec0_finished);
   BOOST_TEST(exec1_finished);
}

// connection::cancel(operation::exec) works. Pending requests are cancelled,
// but written requests are not
void test_cancel_operation_exec()
{
   // Setup
   net::io_context ctx;
   connection conn{ctx};
   bool run_finished = false, exec0_finished = false, exec1_finished = false,
        exec2_finished = false;

   request req0;
   req0.push("PING", "before_blpop");

   request req1;
   req1.push("BLPOP", "any", 1);
   generic_response r1;

   request req2;
   req2.push("PING", "after_blpop");

   // Run the connection
   conn.async_run(make_test_config(), [&](error_code ec) {
      BOOST_TEST_EQ(ec, net::error::operation_aborted);
      run_finished = true;
   });

   // Execute req0 and req1. They will be coalesced together.
   // When req0 completes, we know that req1 will be waiting its response
   conn.async_exec(req0, ignore, [&](error_code ec, std::size_t) {
      BOOST_TEST_EQ(ec, error_code());
      exec0_finished = true;
      conn.cancel(operation::exec);
   });

   // By default, ignore will issue an error when a NULL is received.
   // ATM, this causes the connection to be torn down. Using a generic_response avoids this.
   // See https://github.com/boostorg/redis/issues/314
   conn.async_exec(req1, r1, [&](error_code ec, std::size_t) {
      // No error should occur since the cancellation should be ignored
      std::cout << "async_exec (1): " << ec.message() << std::endl;
      BOOST_TEST_EQ(ec, error_code());
      exec1_finished = true;

      // The connection remains usable
      conn.async_exec(req2, ignore, [&](error_code ec2, std::size_t) {
         BOOST_TEST_EQ(ec2, error_code());
         exec2_finished = true;
         conn.cancel();
      });
   });

   ctx.run_for(test_timeout);
   BOOST_TEST(run_finished);
   BOOST_TEST(exec0_finished);
   BOOST_TEST(exec1_finished);
   BOOST_TEST(exec2_finished);
}

}  // namespace

int main()
{
   test_cancel_pending();
   test_cancel_written();
   test_cancel_if_not_connected();
   test_cancel_on_connection_lost_written();
   test_cancel_operation_exec();

   return boost::report_errors();
}
