//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/connection.hpp>
#include <boost/redis/logger.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/response.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/system/error_code.hpp>

#include "common.hpp"

#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

namespace asio = boost::asio;
namespace redis = boost::redis;
using namespace std::chrono_literals;
using boost::system::error_code;

namespace {

// Creates a user with a known password. Harmless if the user already exists
void setup_password()
{
   // Setup
   asio::io_context ioc;
   redis::connection conn{ioc};

   // Enable the user and grant them permissions on everything
   redis::request req;
   req.push("ACL", "SETUSER", "myuser", "on", ">mypass", "~*", "&*", "+@all");
   redis::generic_response resp;

   bool run_finished = false, exec_finished = false;
   conn.async_run(make_test_config(), [&](error_code ec) {
      run_finished = true;
      BOOST_TEST_EQ(ec, asio::error::operation_aborted);
   });

   conn.async_exec(req, resp, [&](error_code ec, std::size_t) {
      exec_finished = true;
      BOOST_TEST_EQ(ec, error_code());
      conn.cancel();
   });

   ioc.run_for(test_timeout);

   BOOST_TEST(run_finished);
   BOOST_TEST(exec_finished);
   BOOST_TEST(resp.has_value());
}

void test_auth_success()
{
   // Setup
   asio::io_context ioc;
   redis::connection conn{ioc};

   // This request should return the username we're logged in as
   redis::request req;
   req.push("ACL", "WHOAMI");
   redis::response<std::string> resp;

   // These credentials are set up in main, before tests are run
   auto cfg = make_test_config();
   cfg.username = "myuser";
   cfg.password = "mypass";

   bool exec_finished = false, run_finished = false;

   conn.async_exec(req, resp, [&](error_code ec, std::size_t) {
      exec_finished = true;
      BOOST_TEST_EQ(ec, error_code());
      conn.cancel();
   });

   conn.async_run(cfg, [&](error_code ec) {
      run_finished = true;
      BOOST_TEST_EQ(ec, asio::error::operation_aborted);
   });

   ioc.run_for(test_timeout);

   BOOST_TEST(exec_finished);
   BOOST_TEST(run_finished);
   BOOST_TEST_EQ(std::get<0>(resp).value(), "myuser");
}

void test_auth_failure()
{
   // Verify that we log appropriately (see https://github.com/boostorg/redis/issues/297)
   std::ostringstream oss;
   redis::logger lgr(redis::logger::level::info, [&](redis::logger::level, std::string_view msg) {
      oss << msg << '\n';
   });

   // Setup
   asio::io_context ioc;
   redis::connection conn{ioc, std::move(lgr)};

   // Disable reconnection so the hello error causes the connection to exit
   auto cfg = make_test_config();
   cfg.username = "myuser";
   cfg.password = "wrongpass";  // wrong
   cfg.reconnect_wait_interval = 0s;

   bool run_finished = false;

   conn.async_run(cfg, [&](error_code ec) {
      run_finished = true;
      BOOST_TEST_EQ(ec, redis::error::resp3_hello);
   });

   ioc.run_for(test_timeout);

   BOOST_TEST(run_finished);

   // Check the log
   auto log = oss.str();
   if (!BOOST_TEST_NE(log.find("WRONGPASS"), std::string::npos)) {
      std::cerr << "Log was: " << log << std::endl;
   }
}

void test_database_index()
{
   // Setup
   asio::io_context ioc;
   redis::connection conn(ioc);

   // Use a non-default database index
   auto cfg = make_test_config();
   cfg.database_index = 2;

   redis::request req;
   req.push("CLIENT", "INFO");

   redis::response<std::string> resp;

   bool exec_finished = false, run_finished = false;

   conn.async_exec(req, resp, [&](error_code ec, std::size_t n) {
      BOOST_TEST_EQ(ec, error_code());
      std::clog << "async_exec has completed: " << n << std::endl;
      conn.cancel();
      exec_finished = true;
   });

   conn.async_run(cfg, {}, [&run_finished](error_code) {
      std::clog << "async_run has exited." << std::endl;
      run_finished = true;
   });

   ioc.run_for(test_timeout);
   BOOST_TEST(exec_finished);
   BOOST_TEST(run_finished);
   BOOST_TEST_EQ(find_client_info(std::get<0>(resp).value(), "db"), "2");
}

// The user configured an empty setup request. No request should be sent
void test_setup_empty()
{
   // Setup
   asio::io_context ioc;
   redis::connection conn(ioc);

   auto cfg = make_test_config();
   cfg.use_setup = true;
   cfg.setup.clear();

   redis::request req;
   req.push("CLIENT", "INFO");

   redis::response<std::string> resp;

   bool exec_finished = false, run_finished = false;

   conn.async_exec(req, resp, [&](error_code ec, std::size_t) {
      BOOST_TEST_EQ(ec, error_code());
      conn.cancel();
      exec_finished = true;
   });

   conn.async_run(cfg, {}, [&run_finished](error_code) {
      run_finished = true;
   });

   ioc.run_for(test_timeout);
   BOOST_TEST(exec_finished);
   BOOST_TEST(run_finished);
   BOOST_TEST_EQ(find_client_info(std::get<0>(resp).value(), "resp"), "2");  // using RESP2
}

// We can use the setup member to run commands at startup
void test_setup_hello()
{
   // Setup
   asio::io_context ioc;
   redis::connection conn(ioc);

   auto cfg = make_test_config();
   cfg.use_setup = true;
   cfg.setup.clear();
   cfg.setup.push("HELLO", "3", "AUTH", "myuser", "mypass");
   cfg.setup.push("SELECT", 8);

   redis::request req;
   req.push("CLIENT", "INFO");

   redis::response<std::string> resp;

   bool exec_finished = false, run_finished = false;

   conn.async_exec(req, resp, [&](error_code ec, std::size_t) {
      BOOST_TEST_EQ(ec, error_code());
      conn.cancel();
      exec_finished = true;
   });

   conn.async_run(cfg, {}, [&run_finished](error_code) {
      run_finished = true;
   });

   ioc.run_for(test_timeout);
   BOOST_TEST(exec_finished);
   BOOST_TEST(run_finished);
   BOOST_TEST_EQ(find_client_info(std::get<0>(resp).value(), "resp"), "3");  // using RESP3
   BOOST_TEST_EQ(find_client_info(std::get<0>(resp).value(), "user"), "myuser");
   BOOST_TEST_EQ(find_client_info(std::get<0>(resp).value(), "db"), "8");
}

// Running a pipeline without a HELLO is okay (regression check: we set the priority flag)
void test_setup_no_hello()
{
   // Setup
   asio::io_context ioc;
   redis::connection conn(ioc);

   auto cfg = make_test_config();
   cfg.use_setup = true;
   cfg.setup.clear();
   cfg.setup.push("SELECT", 8);

   redis::request req;
   req.push("CLIENT", "INFO");

   redis::response<std::string> resp;

   bool exec_finished = false, run_finished = false;

   conn.async_exec(req, resp, [&](error_code ec, std::size_t) {
      BOOST_TEST_EQ(ec, error_code());
      conn.cancel();
      exec_finished = true;
   });

   conn.async_run(cfg, {}, [&run_finished](error_code) {
      run_finished = true;
   });

   ioc.run_for(test_timeout);
   BOOST_TEST(exec_finished);
   BOOST_TEST(run_finished);
   BOOST_TEST_EQ(find_client_info(std::get<0>(resp).value(), "resp"), "2");  // using RESP3
   BOOST_TEST_EQ(find_client_info(std::get<0>(resp).value(), "db"), "8");
}

void test_setup_failure()
{
   // Verify that we log appropriately (see https://github.com/boostorg/redis/issues/297)
   std::ostringstream oss;
   redis::logger lgr(redis::logger::level::info, [&](redis::logger::level, std::string_view msg) {
      oss << msg << '\n';
   });

   // Setup
   asio::io_context ioc;
   redis::connection conn{ioc, std::move(lgr)};

   // Disable reconnection so the hello error causes the connection to exit
   auto cfg = make_test_config();
   cfg.use_setup = true;
   cfg.setup.clear();
   cfg.setup.push("GET", "two", "args");  // GET only accepts one arg, so this will fail
   cfg.reconnect_wait_interval = 0s;

   bool run_finished = false;

   conn.async_run(cfg, [&](error_code ec) {
      run_finished = true;
      BOOST_TEST_EQ(ec, redis::error::resp3_hello);
   });

   ioc.run_for(test_timeout);

   BOOST_TEST(run_finished);

   // Check the log
   auto log = oss.str();
   if (!BOOST_TEST_NE(log.find("wrong number of arguments"), std::string::npos)) {
      std::cerr << "Log was: " << log << std::endl;
   }
}

}  // namespace

int main()
{
   setup_password();
   test_auth_success();
   test_auth_failure();
   test_database_index();
   test_setup_empty();
   test_setup_hello();
   test_setup_no_hello();
   test_setup_failure();

   return boost::report_errors();
}
