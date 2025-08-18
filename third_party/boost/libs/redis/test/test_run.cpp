/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/connection.hpp>

#include <boost/system/error_code.hpp>
#define BOOST_TEST_MODULE run
#include <boost/test/included/unit_test.hpp>

#include "common.hpp"

namespace net = boost::asio;
namespace redis = boost::redis;

using connection = redis::connection;
using redis::config;
using redis::logger;
using redis::operation;
using boost::system::error_code;
using namespace std::chrono_literals;

namespace {

bool is_host_not_found(error_code ec)
{
   if (ec == net::error::netdb_errors::host_not_found)
      return true;
   if (ec == net::error::netdb_errors::host_not_found_try_again)
      return true;
   return false;
}

BOOST_AUTO_TEST_CASE(resolve_bad_host)
{
   net::io_context ioc;
   connection conn{ioc};

   auto cfg = make_test_config();
   cfg.addr.host = "Atibaia";
   cfg.addr.port = "6379";
   cfg.resolve_timeout = 10h;
   cfg.connect_timeout = 10h;
   cfg.health_check_interval = 10h;
   cfg.reconnect_wait_interval = 0s;

   bool run_finished = true;
   conn.async_run(cfg, {}, [&run_finished](error_code ec) {
      run_finished = true;
      BOOST_TEST(is_host_not_found(ec), "is_host_not_found(ec) is false, with ec = " << ec);
   });

   ioc.run_for(4 * test_timeout);
   BOOST_TEST(run_finished);
}

BOOST_AUTO_TEST_CASE(resolve_with_timeout)
{
   net::io_context ioc;
   connection conn{ioc};

   auto cfg = make_test_config();
   cfg.addr.host = "occase.de";
   cfg.addr.port = "6379";
   cfg.resolve_timeout = 1ms;
   cfg.connect_timeout = 1ms;
   cfg.health_check_interval = 10h;
   cfg.reconnect_wait_interval = 0s;

   bool run_finished = true;
   conn.async_run(cfg, {}, [&run_finished](error_code ec) {
      run_finished = true;
      BOOST_TEST(ec != error_code());
   });

   ioc.run_for(4 * test_timeout);
   BOOST_TEST(run_finished);
}

BOOST_AUTO_TEST_CASE(connect_bad_port)
{
   net::io_context ioc;
   connection conn{ioc};

   auto cfg = make_test_config();
   cfg.addr.host = "127.0.0.1";
   cfg.addr.port = "1";
   cfg.resolve_timeout = 10h;
   cfg.connect_timeout = 10s;
   cfg.health_check_interval = 10h;
   cfg.reconnect_wait_interval = 0s;

   bool run_finished = true;
   conn.async_run(cfg, {}, [&run_finished](error_code ec) {
      run_finished = true;
      BOOST_TEST(ec != error_code());
   });

   ioc.run_for(4 * test_timeout);
   BOOST_TEST(run_finished);
}

// Hard to test.
//BOOST_AUTO_TEST_CASE(connect_with_timeout)
//{
//   net::io_context ioc;
//
//   config cfg;
//   cfg.addr.host = "example.com";
//   cfg.addr.port = "80";
//   cfg.resolve_timeout = 10s;
//   cfg.connect_timeout = 1ns;
//   cfg.health_check_interval = 10h;
//
//   auto conn = std::make_shared<connection>(ioc);
//   run(conn, cfg, boost::redis::error::connect_timeout);
//   ioc.run();
//}

}  // namespace
