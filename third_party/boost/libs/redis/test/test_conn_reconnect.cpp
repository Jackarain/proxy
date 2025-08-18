/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/config.hpp>
#include <boost/redis/connection.hpp>

#include <boost/system/error_code.hpp>

#define BOOST_TEST_MODULE conn_reconnect
#include <boost/test/included/unit_test.hpp>

#include "common.hpp"

#include <iostream>

#ifdef BOOST_ASIO_HAS_CO_AWAIT
#include <boost/asio/experimental/awaitable_operators.hpp>

namespace net = boost::asio;
using boost::system::error_code;
using boost::redis::request;
using boost::redis::response;
using boost::redis::ignore;
using boost::redis::logger;
using boost::redis::operation;
using boost::redis::connection;
using namespace std::chrono_literals;
using namespace boost::asio::experimental::awaitable_operators;

namespace {

net::awaitable<void> test_reconnect_impl()
{
   auto ex = co_await net::this_coro::executor;

   request quit_req;
   quit_req.push("QUIT");

   // cancel_on_connection_lost is required because async_run might detect the failure
   // after the 2nd async_exec is issued
   request regular_req;
   regular_req.push("GET", "mykey");
   regular_req.get_config().cancel_on_connection_lost = false;

   auto conn = std::make_shared<connection>(ex);
   auto cfg = make_test_config();
   cfg.reconnect_wait_interval = 100ms;  // make the test run faster
   run(conn, std::move(cfg));

   for (int i = 0; i < 3; ++i) {
      BOOST_TEST_CONTEXT("i=" << i)
      {
         // Issue a quit request, which will cause the server to close the connection.
         // This request will fail
         error_code ec;
         co_await conn->async_exec(quit_req, ignore, net::redirect_error(ec));
         BOOST_TEST(ec == error_code());

         // This should trigger reconnection, which will now succeed.
         // We should be able to execute requests successfully now.
         // TODO: this is currently unreliable - find our why and fix
         co_await conn->async_exec(regular_req, ignore, net::redirect_error(ec));
         // BOOST_TEST(ec == error_code());
      }
   }

   conn->cancel();
}

// Test whether the client works after a reconnect.
BOOST_AUTO_TEST_CASE(test_reconnect)
{
   run_coroutine_test(test_reconnect_impl(), 5 * test_timeout);
}

auto async_test_reconnect_timeout() -> net::awaitable<void>
{
   auto ex = co_await net::this_coro::executor;

   net::steady_timer st{ex};

   auto conn = std::make_shared<connection>(ex);
   error_code ec1, ec3;

   request req1;
   req1.get_config().cancel_if_not_connected = false;
   req1.get_config().cancel_on_connection_lost = true;
   req1.get_config().cancel_if_unresponded = true;
   req1.push("BLPOP", "any", 0);

   st.expires_after(std::chrono::seconds{1});
   auto cfg = make_test_config();
   co_await (conn->async_exec(req1, ignore, redir(ec1)) || st.async_wait(redir(ec3)));

   //BOOST_TEST(!ec1);
   //BOOST_TEST(!ec3);

   request req2;
   req2.get_config().cancel_if_not_connected = false;
   req2.get_config().cancel_on_connection_lost = true;
   req2.get_config().cancel_if_unresponded = true;
   req2.push("QUIT");

   st.expires_after(std::chrono::seconds{1});
   co_await (
      conn->async_exec(req1, ignore, net::redirect_error(net::use_awaitable, ec1)) ||
      st.async_wait(net::redirect_error(net::use_awaitable, ec3)));
   conn->cancel();

   std::cout << "ccc" << std::endl;

   BOOST_CHECK_EQUAL(ec1, boost::asio::error::operation_aborted);
}

BOOST_AUTO_TEST_CASE(test_reconnect_and_idle)
{
   run_coroutine_test(async_test_reconnect_timeout());
}

}  // namespace

#else
BOOST_AUTO_TEST_CASE(dummy) { }
#endif
