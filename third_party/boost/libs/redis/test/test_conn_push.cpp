/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/connection.hpp>
#include <boost/redis/ignore.hpp>
#include <boost/redis/logger.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/resp3/flat_tree.hpp>
#include <boost/redis/resp3/node.hpp>
#include <boost/redis/response.hpp>

#include <boost/asio/error.hpp>
#include <boost/asio/experimental/channel_error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/core/lightweight_test.hpp>

#include "common.hpp"

#include <cstddef>
#include <functional>

namespace net = boost::asio;
using namespace boost::redis;
using namespace std::chrono_literals;
using boost::system::error_code;

// Focuses on the deprecated async_receive and receive
// functions. test_conn_push2 covers the newer receive functionality.

namespace {

// async_receive is outstanding when a push is received
void test_async_receive_waiting_for_push()
{
   resp3::flat_tree resp;
   net::io_context ioc;
   connection conn{ioc};
   conn.set_receive_response(resp);

   request req1;
   req1.push("PING", "Message1");
   req1.push("SUBSCRIBE", "test_async_receive_waiting_for_push");

   request req2;
   req2.push("PING", "Message2");

   bool run_finished = false, push_received = false, exec1_finished = false, exec2_finished = false;

   auto on_exec2 = [&](error_code ec2, std::size_t) {
      BOOST_TEST_EQ(ec2, error_code());
      exec2_finished = true;
      conn.cancel();
   };

   conn.async_exec(req1, ignore, [&](error_code ec, std::size_t) {
      BOOST_TEST_EQ(ec, error_code());
      exec1_finished = true;
   });

   conn.async_receive([&](error_code ec, std::size_t) {
      BOOST_TEST_EQ(ec, error_code());
      BOOST_TEST_EQ(resp.get_total_msgs(), 1u);
      push_received = true;
      conn.async_exec(req2, ignore, on_exec2);
   });

   conn.async_run(make_test_config(), [&](error_code ec) {
      BOOST_TEST_EQ(ec, net::error::operation_aborted);
      run_finished = true;
   });

   ioc.run_for(test_timeout);

   BOOST_TEST(push_received);
   BOOST_TEST(exec1_finished);
   BOOST_TEST(exec2_finished);
   BOOST_TEST(run_finished);
}

// A push is already available when async_receive is called
void test_async_receive_push_available()
{
   net::io_context ioc;
   connection conn{ioc};
   resp3::flat_tree resp;
   conn.set_receive_response(resp);

   // SUBSCRIBE doesn't have a response, but causes a push to be delivered.
   // Add a PING so the overall request has a response.
   // This ensures that when async_exec completes, the push has been delivered
   request req;
   req.push("SUBSCRIBE", "test_async_receive_push_available");
   req.push("PING", "message");

   bool push_received = false, exec_finished = false, run_finished = false;

   auto on_receive = [&](error_code ec, std::size_t) {
      push_received = true;
      BOOST_TEST_EQ(ec, error_code());
      BOOST_TEST_EQ(resp.get_total_msgs(), 1u);
      conn.cancel();
   };

   conn.async_exec(req, ignore, [&](error_code ec, std::size_t) {
      exec_finished = true;
      BOOST_TEST_EQ(ec, error_code());
      conn.async_receive(on_receive);
   });

   conn.async_run(make_test_config(), [&](error_code ec) {
      run_finished = true;
      BOOST_TEST_EQ(ec, net::error::operation_aborted);
   });

   ioc.run_for(test_timeout);

   BOOST_TEST(exec_finished);
   BOOST_TEST(push_received);
   BOOST_TEST(run_finished);
}

// Synchronous receive can be used to try to read a message
void test_sync_receive()
{
   net::io_context ioc;
   connection conn{ioc};
   resp3::flat_tree resp;
   conn.set_receive_response(resp);

   // Subscribing to 2 channels causes 2 pushes to be delivered.
   // Adding a PING guarantees that after exec finishes, the push has been read
   request req;
   req.push("SUBSCRIBE", "test_sync_receive_channel1");
   req.push("SUBSCRIBE", "test_sync_receive_channel2");
   req.push("PING", "message");

   bool exec_finished = false, run_finished = false;

   conn.async_exec(req, ignore, [&](error_code ec, std::size_t) {
      exec_finished = true;
      BOOST_TEST_EQ(ec, error_code());

      // At this point, the receive response contains all the pushes
      BOOST_TEST_EQ(resp.get_total_msgs(), 2u);

      // Receive the 1st push synchronously
      std::size_t push_bytes = conn.receive(ec);
      BOOST_TEST_EQ(ec, error_code());
      BOOST_TEST_GT(push_bytes, 0u);

      // Receive the 2nd push synchronously
      push_bytes = conn.receive(ec);
      BOOST_TEST_EQ(ec, error_code());
      BOOST_TEST_GT(push_bytes, 0u);

      // There are no more pushes. Trying to receive one more fails
      push_bytes = conn.receive(ec);
      BOOST_TEST_EQ(ec, error::sync_receive_push_failed);
      BOOST_TEST_EQ(push_bytes, 0u);

      conn.cancel();
   });

   conn.async_run(make_test_config(), [&](error_code ec) {
      BOOST_TEST_EQ(ec, net::error::operation_aborted);
      run_finished = true;
   });

   // Trying to receive a push before one is received fails
   error_code ec;
   std::size_t push_bytes = conn.receive(ec);
   BOOST_TEST_EQ(ec, error::sync_receive_push_failed);
   BOOST_TEST_EQ(push_bytes, 0u);

   ioc.run_for(test_timeout);

   BOOST_TEST(exec_finished);
   BOOST_TEST(run_finished);
}

// async_receive is cancelled every time a reconnection happens,
// so we can re-establish subscriptions
struct test_async_receive_cancelled_on_reconnection_impl {
   net::io_context ioc;
   connection conn{ioc};
   resp3::flat_tree resp{};
   request req_subscribe{}, req_quit{};
   bool receive_finished = false, quit_finished = false;

   // Subscribe to a channel. This will cause a push to be received
   void start_subscribe1()
   {
      conn.async_exec(req_subscribe, ignore, [this](error_code ec, std::size_t) {
         BOOST_TEST_EQ(ec, error_code());
         start_receive1();
      });
   }

   // Receive the push triggered by the subscribe
   void start_receive1()
   {
      conn.async_receive([this](error_code ec, std::size_t) {
         BOOST_TEST_EQ(ec, error_code());
         BOOST_TEST_EQ(resp.get_total_msgs(), 1u);
         resp.clear();

         // In parallel, trigger a reconnection and start a receive operation
         start_receive_reconnection();
         start_quit();
      });
   }

   // The next receive operation will be cancelled by the reconnection
   void start_receive_reconnection()
   {
      conn.async_receive([this](error_code ec, std::size_t) {
         BOOST_TEST_EQ(ec, net::experimental::channel_errc::channel_cancelled);
         BOOST_TEST_EQ(resp.get_total_msgs(), 0u);
         start_subscribe2();
      });
   }

   // Trigger a reconnection. This is a "leaf" operation
   void start_quit()
   {
      conn.async_exec(req_quit, ignore, [this](error_code, std::size_t) {
         quit_finished = true;
      });
   }

   // Resubscribe after the reconnection
   void start_subscribe2()
   {
      conn.async_exec(req_subscribe, ignore, [this](error_code ec, std::size_t) {
         BOOST_TEST_EQ(ec, error_code());
         start_receive2();
      });
   }

   // Receive the push triggered by the 2nd subscribe
   void start_receive2()
   {
      conn.async_receive([this](error_code ec, std::size_t) {
         BOOST_TEST_EQ(ec, error_code());
         BOOST_TEST_EQ(resp.get_total_msgs(), 1u);
         receive_finished = true;
         conn.cancel();
      });
   }

   void run()
   {
      req_subscribe.push("SUBSCRIBE", "test_async_receive_cancelled_on_reconnection");
      req_subscribe.push("PING");

      req_quit.push("QUIT");

      conn.set_receive_response(resp);

      bool run_finished = false;

      start_subscribe1();

      conn.async_run(make_test_config(), [&](error_code ec) {
         run_finished = true;
         BOOST_TEST_EQ(ec, net::error::operation_aborted);
      });

      ioc.run_for(test_timeout);

      BOOST_TEST(run_finished);
      BOOST_TEST(receive_finished);
      BOOST_TEST(quit_finished);
   }
};

void test_async_receive_cancelled_on_reconnection()
{
   test_async_receive_cancelled_on_reconnection_impl{}.run();
}

// After an async_receive operation finishes, another one can be issued
void test_consecutive_receives()
{
   net::io_context ioc;
   connection conn{ioc};
   resp3::flat_tree resp;
   bool push_consumer_finished{false};

   std::function<void()> launch_push_consumer = [&]() {
      conn.async_receive([&](error_code ec, std::size_t) {
         if (ec) {
            BOOST_TEST_EQ(ec, net::experimental::error::channel_cancelled);
            push_consumer_finished = true;
            resp.clear();
            return;
         }
         launch_push_consumer();
      });
   };

   conn.set_receive_response(resp);

   request req1;
   req1.get_config().cancel_on_connection_lost = false;
   req1.push("PING", "Message1");

   request req2;
   req2.get_config().cancel_on_connection_lost = false;
   req2.push("SUBSCRIBE", "channel");

   bool exec_finished = false, run_finished = false;

   auto c10 = [&](error_code ec, std::size_t) {
      BOOST_TEST_EQ(ec, error_code());
      exec_finished = true;
      conn.cancel();
   };
   auto c9 = [&](error_code ec, std::size_t) {
      BOOST_TEST_EQ(ec, error_code());
      conn.async_exec(req2, ignore, c10);
   };
   auto c8 = [&](error_code ec, std::size_t) {
      BOOST_TEST_EQ(ec, error_code());
      conn.async_exec(req1, ignore, c9);
   };
   auto c7 = [&](error_code ec, std::size_t) {
      BOOST_TEST_EQ(ec, error_code());
      conn.async_exec(req2, ignore, c8);
   };
   auto c6 = [&](error_code ec, std::size_t) {
      BOOST_TEST_EQ(ec, error_code());
      conn.async_exec(req2, ignore, c7);
   };
   auto c5 = [&](error_code ec, std::size_t) {
      BOOST_TEST_EQ(ec, error_code());
      conn.async_exec(req1, ignore, c6);
   };
   auto c4 = [&](error_code ec, std::size_t) {
      BOOST_TEST_EQ(ec, error_code());
      conn.async_exec(req2, ignore, c5);
   };
   auto c3 = [&](error_code ec, std::size_t) {
      BOOST_TEST_EQ(ec, error_code());
      conn.async_exec(req1, ignore, c4);
   };
   auto c2 = [&](error_code ec, std::size_t) {
      BOOST_TEST_EQ(ec, error_code());
      conn.async_exec(req2, ignore, c3);
   };
   auto c1 = [&](error_code ec, std::size_t) {
      BOOST_TEST_EQ(ec, error_code());
      conn.async_exec(req2, ignore, c2);
   };

   conn.async_exec(req1, ignore, c1);
   launch_push_consumer();

   conn.async_run(make_test_config(), [&](error_code ec) {
      run_finished = true;
      BOOST_TEST_EQ(ec, net::error::operation_aborted);
   });

   ioc.run_for(test_timeout);

   BOOST_TEST(exec_finished);
   BOOST_TEST(run_finished);
   BOOST_TEST(push_consumer_finished);
};

}  // namespace

int main()
{
   test_async_receive_waiting_for_push();
   test_async_receive_push_available();
   test_sync_receive();
   test_async_receive_cancelled_on_reconnection();
   test_consecutive_receives();

   return boost::report_errors();
}
