/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/connection.hpp>

#include <cstddef>
#define BOOST_TEST_MODULE conn_exec_cancel
#include <boost/test/included/unit_test.hpp>

#include "common.hpp"

#include <iostream>

#ifdef BOOST_ASIO_HAS_CO_AWAIT

// NOTE1: Sends hello separately. I have observed that if hello and
// blpop are sent toguether, Redis will send the response of hello
// right away, not waiting for blpop. That is why we have to send it
// separately.

namespace net = boost::asio;
using error_code = boost::system::error_code;
using boost::redis::operation;
using boost::redis::request;
using boost::redis::response;
using boost::redis::generic_response;
using boost::redis::ignore;
using boost::redis::ignore_t;
using boost::redis::config;
using boost::redis::logger;
using boost::redis::connection;
using namespace std::chrono_literals;

namespace {

auto async_ignore_explicit_cancel_of_req_written() -> net::awaitable<void>
{
   auto ex = co_await net::this_coro::executor;

   generic_response gresp;
   auto conn = std::make_shared<connection>(ex);

   run(conn);

   net::steady_timer st{ex};
   st.expires_after(std::chrono::seconds{1});

   // See NOTE1.
   request req0;
   req0.push("PING", "async_ignore_explicit_cancel_of_req_written");
   co_await conn->async_exec(req0, gresp);

   request req1;
   req1.push("BLPOP", "any", 3);

   bool seen = false;
   conn->async_exec(req1, gresp, [&](error_code ec, std::size_t) {
      // No error should occur since the cancellation should be ignored
      std::cout << "async_exec (1): " << ec.message() << std::endl;
      BOOST_TEST(ec == error_code());
      seen = true;
   });

   // Will complete while BLPOP is pending.
   error_code ec;
   co_await st.async_wait(net::redirect_error(ec));
   conn->cancel(operation::exec);

   BOOST_TEST(ec == error_code());

   request req2;
   req2.push("PING");

   // Test whether the connection remains usable after a call to
   // cancel(exec).
   co_await conn->async_exec(req2, gresp, net::redirect_error(ec));
   conn->cancel();

   BOOST_TEST(ec == error_code());
   BOOST_TEST(seen);
}

BOOST_AUTO_TEST_CASE(test_ignore_explicit_cancel_of_req_written)
{
   run_coroutine_test(async_ignore_explicit_cancel_of_req_written());
}

}  // namespace

#else
BOOST_AUTO_TEST_CASE(dummy) { }
#endif
