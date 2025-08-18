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

#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/local/basic_endpoint.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/system/error_code.hpp>

#include "common.hpp"

#include <cstddef>
#include <iostream>
#include <string>
#include <string_view>

using boost::system::error_code;
namespace net = boost::asio;
using namespace boost::redis;
using namespace std::chrono_literals;
using namespace std::string_view_literals;

namespace {

#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS

constexpr std::string_view unix_socket_path = "/tmp/redis-socks/redis.sock";

// Executing commands using UNIX sockets works
void test_exec()
{
   // Setup
   net::io_context ioc;
   connection conn{ioc};
   auto cfg = make_test_config();
   cfg.unix_socket = unix_socket_path;
   bool run_finished = false, exec_finished = false;

   // Run the connection
   conn.async_run(cfg, {}, [&run_finished](error_code ec) {
      run_finished = true;
      BOOST_TEST_EQ(ec, net::error::operation_aborted);
   });

   // Execute a request
   request req;
   req.push("PING", "unix");
   response<std::string> res;
   conn.async_exec(req, res, [&](error_code ec, std::size_t) {
      exec_finished = true;
      BOOST_TEST_EQ(ec, error_code());
      conn.cancel();
   });

   // Run
   ioc.run_for(test_timeout);

   // Check
   BOOST_TEST(exec_finished);
   BOOST_TEST(run_finished);
   BOOST_TEST_EQ(std::get<0>(res).value(), "unix"sv);
}

// If the connection is lost when using a UNIX socket, we can reconnect
void test_reconnection()
{
   // Setup
   net::io_context ioc;
   connection conn{ioc};
   auto cfg = make_test_config();
   cfg.unix_socket = unix_socket_path;
   cfg.reconnect_wait_interval = 10ms;  // make the test run faster

   request ping_request;
   ping_request.push("PING", "some_value");

   request quit_request;
   quit_request.push("QUIT");

   bool exec_finished = false, run_finished = false;

   // Run the connection
   conn.async_run(cfg, {}, [&](error_code ec) {
      run_finished = true;
      BOOST_TEST(ec == net::error::operation_aborted);
   });

   // The PING is the end of the callback chain
   auto ping_callback = [&](error_code ec, std::size_t) {
      exec_finished = true;
      BOOST_TEST(ec == error_code());
      conn.cancel();
   };

   auto quit_callback = [&](error_code ec, std::size_t) {
      BOOST_TEST(ec == error_code());

      // If a request is issued immediately after QUIT, the request sometimes
      // fails, probably due to a race condition. This dispatches any pending
      // handlers, triggering the reconnection process.
      // TODO: this should not be required.
      ioc.poll();
      conn.async_exec(ping_request, ignore, ping_callback);
   };

   conn.async_exec(quit_request, ignore, quit_callback);

   ioc.run_for(test_timeout);

   BOOST_TEST(exec_finished);
   BOOST_TEST(run_finished);
}

// We can freely switch between UNIX sockets and other transports
void test_switch_between_transports()
{
   // Setup
   net::io_context ioc;
   connection conn{ioc};
   request req;
   response<std::string> res1, res2, res3;
   req.push("PING", "hello");
   bool finished = false;

   // Create configurations for TLS and UNIX connections
   auto tcp_tls_cfg = make_test_config();
   tcp_tls_cfg.use_ssl = true;
   tcp_tls_cfg.addr.port = "6380";
   auto unix_cfg = make_test_config();
   unix_cfg.unix_socket = unix_socket_path;

   // After the last TCP/TLS run, exit
   auto on_run_tls_2 = [&](error_code ec) {
      finished = true;
      std::cout << "Run (TCP/TLS 2) finished\n";
      BOOST_TEST_EQ(ec, net::error::operation_aborted);
   };

   // After UNIX sockets, switch back to TCP/tLS
   auto on_run_unix = [&](error_code ec) {
      std::cout << "Run (UNIX) finished\n";
      BOOST_TEST_EQ(ec, net::error::operation_aborted);

      // Change to using TCP with TLS again
      conn.async_run(unix_cfg, {}, on_run_tls_2);
      conn.async_exec(req, res3, [&](error_code ec, std::size_t) {
         std::cout << "Exec 3 finished\n";
         BOOST_TEST_EQ(ec, error_code());
         BOOST_TEST_EQ(std::get<0>(res3).value(), "hello");
         conn.cancel();
      });
   };

   // After TCP/TLS, change to UNIX sockets
   auto on_run_tls_1 = [&](error_code ec) {
      std::cout << "Run (TCP/TLS 1) finished\n";
      BOOST_TEST_EQ(ec, net::error::operation_aborted);

      conn.async_run(unix_cfg, {}, on_run_unix);
      conn.async_exec(req, res2, [&](error_code ec, std::size_t) {
         std::cout << "Exec 2 finished\n";
         BOOST_TEST_EQ(ec, error_code());
         BOOST_TEST_EQ(std::get<0>(res2).value(), "hello");
         conn.cancel();
      });
   };

   // Start with TCP/TLS
   conn.async_run(tcp_tls_cfg, {}, on_run_tls_1);
   conn.async_exec(req, res1, [&](error_code ec, std::size_t) {
      std::cout << "Exec 1 finished\n";
      BOOST_TEST_EQ(ec, error_code());
      BOOST_TEST_EQ(std::get<0>(res1).value(), "hello");
      conn.cancel();
   });

   // Run the test
   ioc.run_for(test_timeout);
   BOOST_TEST(finished);
}

// Trying to enable TLS and UNIX sockets at the same time
// is an error and makes async_run exit immediately
void test_error_unix_tls()
{
   // Setup
   net::io_context ioc;
   connection conn{ioc};
   auto cfg = make_test_config();
   cfg.use_ssl = true;
   cfg.addr.port = "6380";
   cfg.unix_socket = unix_socket_path;
   bool finished = false;

   // Run the connection
   conn.async_run(cfg, {}, [&finished](error_code ec) {
      BOOST_TEST_EQ(ec, error::unix_sockets_ssl_unsupported);
      finished = true;
   });

   // Run the test
   ioc.run_for(test_timeout);
   BOOST_TEST(finished);
}

#else

// Trying to enable TLS and UNIX sockets at the same time
// is an error and makes async_run exit immediately
void test_unix_not_supported()
{
   // Setup
   net::io_context ioc;
   connection conn{ioc};
   auto cfg = make_test_config();
   cfg.unix_socket = "/some/path.sock";
   bool finished = false;

   // Run the connection
   conn.async_run(cfg, {}, [&finished](error_code ec) {
      BOOST_TEST_EQ(ec, error::unix_sockets_unsupported);
      finished = true;
   });

   // Run the test
   ioc.run_for(test_timeout);
   BOOST_TEST(finished);
}

#endif

}  // namespace

int main()
{
#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
   test_exec();
   test_reconnection();
   test_switch_between_transports();
   test_error_unix_tls();
#else
   test_unix_not_supported();
#endif

   return boost::report_errors();
}
