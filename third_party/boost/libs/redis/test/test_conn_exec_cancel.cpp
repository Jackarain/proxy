/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/connection.hpp>
#include <boost/redis/ignore.hpp>
#include <boost/redis/request.hpp>

#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/system/errc.hpp>

#include <cstddef>
#define BOOST_TEST_MODULE conn_exec_cancel
#include <boost/asio/detached.hpp>
#include <boost/test/included/unit_test.hpp>

#include "common.hpp"

#ifdef BOOST_ASIO_HAS_CO_AWAIT
#include <boost/asio/experimental/awaitable_operators.hpp>

using namespace std::chrono_literals;

// NOTE1: I have observed that if hello and
// blpop are sent together, Redis will send the response of hello
// right away, not waiting for blpop.

namespace net = boost::asio;
using error_code = boost::system::error_code;
using namespace net::experimental::awaitable_operators;
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

auto implicit_cancel_of_req_written() -> net::awaitable<void>
{
   auto ex = co_await net::this_coro::executor;
   auto conn = std::make_shared<connection>(ex);

   auto cfg = make_test_config();
   cfg.health_check_interval = std::chrono::seconds::zero();
   run(conn, cfg);

   // See NOTE1.
   request req0;
   req0.push("PING");
   co_await conn->async_exec(req0, ignore);

   // Will be cancelled after it has been written but before the
   // response arrives.
   request req1;
   req1.push("BLPOP", "any", 3);

   net::steady_timer st{ex};
   st.expires_after(std::chrono::seconds{1});

   // Achieves implicit cancellation when the timer fires.
   boost::system::error_code ec1, ec2;
   co_await (conn->async_exec(req1, ignore, redir(ec1)) || st.async_wait(redir(ec2)));

   conn->cancel();

   // I have observed this produces terminal cancellation so it can't
   // be ignored, an error is expected.
   BOOST_TEST(ec1 == net::error::operation_aborted);
   BOOST_TEST(ec2 == error_code());
}

BOOST_AUTO_TEST_CASE(test_ignore_implicit_cancel_of_req_written)
{
   run_coroutine_test(implicit_cancel_of_req_written());
}

BOOST_AUTO_TEST_CASE(test_cancel_of_req_written_on_run_canceled)
{
   net::io_context ioc;
   auto conn = std::make_shared<connection>(ioc);

   request req0;
   req0.push("PING");

   // Sends a request that will be blocked forever, so we can test
   // canceling it while waiting for a response.
   request req1;
   req1.get_config().cancel_on_connection_lost = true;
   req1.get_config().cancel_if_unresponded = true;
   req1.push("BLPOP", "any", 0);

   bool finished = false;

   auto c1 = [&](error_code ec, std::size_t) {
      BOOST_CHECK_EQUAL(ec, net::error::operation_aborted);
      finished = true;
   };

   auto c0 = [&](error_code ec, std::size_t) {
      BOOST_TEST(ec == error_code());
      conn->async_exec(req1, ignore, c1);
   };

   conn->async_exec(req0, ignore, c0);

   auto cfg = make_test_config();
   cfg.health_check_interval = std::chrono::seconds{5};
   run(conn);

   net::steady_timer st{ioc};
   st.expires_after(std::chrono::seconds{1});
   st.async_wait([&](error_code ec) {
      BOOST_TEST(ec == error_code());
      conn->cancel(operation::run);
      conn->cancel(operation::reconnection);
   });

   ioc.run_for(test_timeout);
   BOOST_TEST(finished);
}

// We can cancel requests that haven't been written yet.
// All cancellation types are supported here.
BOOST_AUTO_TEST_CASE(test_cancel_pending)
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
      BOOST_TEST_CONTEXT(tc.name)
      {
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
               BOOST_TEST(ec == net::error::operation_aborted);
               BOOST_TEST(sz == 0u);
               called = true;
            }));

         // Issue a cancellation
         sig.emit(tc.cancel_type);

         // Prevent the test for deadlocking in case of failure
         ctx.run_for(3s);
         BOOST_TEST(called);
      }
   }
}

}  // namespace

#else
BOOST_AUTO_TEST_CASE(dummy) { }
#endif
