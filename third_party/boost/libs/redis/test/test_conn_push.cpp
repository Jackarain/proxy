/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/connection.hpp>
#include <boost/redis/logger.hpp>

#include <boost/asio/experimental/channel_error.hpp>
#include <boost/system/errc.hpp>

#define BOOST_TEST_MODULE conn_push
#include <boost/test/included/unit_test.hpp>

#include "common.hpp"

#include <cstddef>
#include <iostream>

namespace net = boost::asio;
namespace redis = boost::redis;

using boost::redis::operation;
using boost::redis::connection;
using boost::system::error_code;
using boost::redis::request;
using boost::redis::response;
using boost::redis::ignore;
using boost::redis::ignore_t;
using boost::system::error_code;
using boost::redis::logger;
using namespace std::chrono_literals;

namespace {

BOOST_AUTO_TEST_CASE(receives_push_waiting_resps)
{
   request req1;
   req1.push("HELLO", 3);
   req1.push("PING", "Message1");

   request req2;
   req2.push("SUBSCRIBE", "channel");

   request req3;
   req3.push("PING", "Message2");
   req3.push("QUIT");

   net::io_context ioc;

   auto conn = std::make_shared<connection>(ioc);

   bool push_received = false, c1_called = false, c2_called = false, c3_called = false;

   auto c3 = [&](error_code ec, std::size_t) {
      c3_called = true;
      std::cout << "c3: " << ec.message() << std::endl;
   };

   auto c2 = [&, conn](error_code ec, std::size_t) {
      c2_called = true;
      BOOST_TEST(ec == error_code());
      conn->async_exec(req3, ignore, c3);
   };

   auto c1 = [&, conn](error_code ec, std::size_t) {
      c1_called = true;
      BOOST_TEST(ec == error_code());
      conn->async_exec(req2, ignore, c2);
   };

   conn->async_exec(req1, ignore, c1);

   run(conn, make_test_config(), {});

   conn->async_receive([&, conn](error_code ec, std::size_t) {
      std::cout << "async_receive" << std::endl;
      BOOST_TEST(ec == error_code());
      push_received = true;
      conn->cancel();
   });

   ioc.run_for(test_timeout);

   BOOST_TEST(push_received);
   BOOST_TEST(c1_called);
   BOOST_TEST(c2_called);
   BOOST_TEST(c3_called);
}

BOOST_AUTO_TEST_CASE(push_received1)
{
   net::io_context ioc;
   auto conn = std::make_shared<connection>(ioc);

   // Trick: Uses SUBSCRIBE because this command has no response or
   // better said, its response is a server push, which is what we
   // want to test. We send two because we want to test both
   // async_receive and receive.
   request req;
   req.push("SUBSCRIBE", "channel1");
   req.push("SUBSCRIBE", "channel2");

   bool push_received = false, exec_finished = false;

   conn->async_exec(req, ignore, [&, conn](error_code ec, std::size_t) {
      exec_finished = true;
      std::cout << "async_exec" << std::endl;
      BOOST_TEST(ec == error_code());
   });

   conn->async_receive([&, conn](error_code ec, std::size_t) {
      push_received = true;
      std::cout << "(1) async_receive" << std::endl;

      BOOST_TEST(ec == error_code());

      // Receives the second push synchronously.
      error_code ec2;
      std::size_t res = 0;
      res = conn->receive(ec2);
      BOOST_TEST(!ec2);
      BOOST_TEST(res != std::size_t(0));

      // Tries to receive a third push synchronously.
      ec2 = {};
      res = conn->receive(ec2);
      BOOST_CHECK_EQUAL(
         ec2,
         boost::redis::make_error_code(boost::redis::error::sync_receive_push_failed));

      conn->cancel();
   });

   run(conn);
   ioc.run_for(test_timeout);

   BOOST_TEST(exec_finished);
   BOOST_TEST(push_received);
}

BOOST_AUTO_TEST_CASE(push_filtered_out)
{
   net::io_context ioc;
   auto conn = std::make_shared<connection>(ioc);

   request req;
   req.push("HELLO", 3);
   req.push("PING");
   req.push("SUBSCRIBE", "channel");
   req.push("QUIT");

   response<ignore_t, std::string, std::string> resp;

   bool exec_finished = false, push_received = false;

   conn->async_exec(req, resp, [conn, &exec_finished](error_code ec, std::size_t) {
      exec_finished = true;
      BOOST_TEST(ec == error_code());
   });

   conn->async_receive([&, conn](error_code ec, std::size_t) {
      push_received = true;
      BOOST_TEST(ec == error_code());
      conn->cancel(operation::reconnection);
   });

   run(conn);

   ioc.run_for(test_timeout);
   BOOST_TEST(exec_finished);
   BOOST_TEST(push_received);

   BOOST_CHECK_EQUAL(std::get<1>(resp).value(), "PONG");
   BOOST_CHECK_EQUAL(std::get<2>(resp).value(), "OK");
}

struct response_error_tag { };
response_error_tag error_tag_obj;

struct response_error_adapter {
   void operator()(
      std::size_t,
      boost::redis::resp3::basic_node<std::string_view> const&,
      boost::system::error_code& ec)
   {
      ec = boost::redis::error::incompatible_size;
   }

   [[nodiscard]]
   auto get_supported_response_size() const noexcept
   {
      return static_cast<std::size_t>(-1);
   }
};

auto boost_redis_adapt(response_error_tag&) { return response_error_adapter{}; }

BOOST_AUTO_TEST_CASE(test_push_adapter)
{
   net::io_context ioc;
   auto conn = std::make_shared<connection>(ioc);

   request req;
   req.push("HELLO", 3);
   req.push("PING");
   req.push("SUBSCRIBE", "channel");
   req.push("PING");

   conn->set_receive_response(error_tag_obj);

   bool push_received = false, exec_finished = false, run_finished = false;

   conn->async_receive([&, conn](error_code ec, std::size_t) {
      BOOST_CHECK_EQUAL(ec, boost::asio::experimental::error::channel_cancelled);
      conn->cancel(operation::reconnection);
      push_received = true;
   });

   conn->async_exec(req, ignore, [&exec_finished](error_code ec, std::size_t) {
      BOOST_CHECK_EQUAL(ec, boost::system::errc::errc_t::operation_canceled);
      exec_finished = true;
   });

   auto cfg = make_test_config();
   conn->async_run(cfg, {}, [&run_finished](error_code ec) {
      BOOST_CHECK_EQUAL(ec, redis::error::incompatible_size);
      run_finished = true;
   });

   ioc.run_for(test_timeout);
   BOOST_TEST(push_received);
   BOOST_TEST(exec_finished);
   BOOST_TEST(run_finished);

   // TODO: Reset the ioc reconnect and send a quit to ensure
   // reconnection is possible after an error.
}

void launch_push_consumer(std::shared_ptr<connection> conn)
{
   conn->async_receive([conn](error_code ec, std::size_t) {
      if (ec) {
         BOOST_TEST(ec == net::experimental::error::channel_cancelled);
         return;
      }
      launch_push_consumer(conn);
   });
}

BOOST_AUTO_TEST_CASE(many_subscribers)
{
   request req0;
   req0.get_config().cancel_on_connection_lost = false;
   req0.push("HELLO", 3);

   request req1;
   req1.get_config().cancel_on_connection_lost = false;
   req1.push("PING", "Message1");

   request req2;
   req2.get_config().cancel_on_connection_lost = false;
   req2.push("SUBSCRIBE", "channel");

   request req3;
   req3.get_config().cancel_on_connection_lost = false;
   req3.push("QUIT");

   net::io_context ioc;
   auto conn = std::make_shared<connection>(ioc);

   bool finished = false;

   auto c11 = [&](error_code ec, std::size_t) {
      BOOST_TEST(ec == error_code());
      conn->cancel(operation::reconnection);
      finished = true;
   };
   auto c10 = [&](error_code ec, std::size_t) {
      BOOST_TEST(ec == error_code());
      conn->async_exec(req3, ignore, c11);
   };
   auto c9 = [&](error_code ec, std::size_t) {
      BOOST_TEST(ec == error_code());
      conn->async_exec(req2, ignore, c10);
   };
   auto c8 = [&](error_code ec, std::size_t) {
      BOOST_TEST(ec == error_code());
      conn->async_exec(req1, ignore, c9);
   };
   auto c7 = [&](error_code ec, std::size_t) {
      BOOST_TEST(ec == error_code());
      conn->async_exec(req2, ignore, c8);
   };
   auto c6 = [&](error_code ec, std::size_t) {
      BOOST_TEST(ec == error_code());
      conn->async_exec(req2, ignore, c7);
   };
   auto c5 = [&](error_code ec, std::size_t) {
      BOOST_TEST(ec == error_code());
      conn->async_exec(req1, ignore, c6);
   };
   auto c4 = [&](error_code ec, std::size_t) {
      BOOST_TEST(ec == error_code());
      conn->async_exec(req2, ignore, c5);
   };
   auto c3 = [&](error_code ec, std::size_t) {
      BOOST_TEST(ec == error_code());
      conn->async_exec(req1, ignore, c4);
   };
   auto c2 = [&](error_code ec, std::size_t) {
      BOOST_TEST(ec == error_code());
      conn->async_exec(req2, ignore, c3);
   };
   auto c1 = [&](error_code ec, std::size_t) {
      BOOST_TEST(ec == error_code());
      conn->async_exec(req2, ignore, c2);
   };
   auto c0 = [&](error_code ec, std::size_t) {
      BOOST_TEST(ec == error_code());
      conn->async_exec(req1, ignore, c1);
   };

   conn->async_exec(req0, ignore, c0);
   launch_push_consumer(conn);

   run(conn, make_test_config(), {});

   ioc.run_for(test_timeout);
   BOOST_TEST(finished);
}

}  // namespace
