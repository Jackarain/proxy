//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/detail/connection_state.hpp>
#include <boost/redis/impl/setup_request_utils.hpp>
#include <boost/redis/resp3/parser.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/system/detail/error_code.hpp>
#include <boost/system/result.hpp>

#include <string_view>

using namespace boost::redis;
using detail::setup_adapter;
using detail::connection_state;
using detail::compose_setup_request;
using boost::system::error_code;

namespace {

void test_success()
{
   // Setup
   connection_state st;
   st.cfg.use_setup = true;
   st.cfg.setup.push("SELECT", 2);
   compose_setup_request(st.cfg, st.tracker, st.setup_req);
   setup_adapter adapter{st};

   // Response to HELLO
   resp3::parser p;
   error_code ec;
   bool done = resp3::parse(p, "%1\r\n$6\r\nserver\r\n$5\r\nredis\r\n", adapter, ec);
   BOOST_TEST(done);
   BOOST_TEST_EQ(ec, error_code());

   // Response to the SELECT command
   p.reset();
   done = resp3::parse(p, "+OK\r\n", adapter, ec);
   BOOST_TEST(done);
   BOOST_TEST_EQ(ec, error_code());

   // No diagnostic
   BOOST_TEST_EQ(st.diagnostic, "");
}

void test_simple_error()
{
   // Setup
   connection_state st;
   st.cfg.use_setup = true;
   compose_setup_request(st.cfg, st.tracker, st.setup_req);
   setup_adapter adapter{st};

   // Response to HELLO contains an error
   resp3::parser p;
   error_code ec;
   bool done = resp3::parse(p, "-ERR unauthorized\r\n", adapter, ec);
   BOOST_TEST(done);
   BOOST_TEST_EQ(ec, error::resp3_hello);
   BOOST_TEST_EQ(st.diagnostic, "ERR unauthorized");
}

void test_blob_error()
{
   // Setup
   connection_state st;
   st.cfg.use_setup = true;
   st.cfg.setup.push("SELECT", 1);
   compose_setup_request(st.cfg, st.tracker, st.setup_req);
   setup_adapter adapter{st};

   // Response to HELLO
   resp3::parser p;
   error_code ec;
   bool done = resp3::parse(p, "%1\r\n$6\r\nserver\r\n$5\r\nredis\r\n", adapter, ec);
   BOOST_TEST(done);
   BOOST_TEST_EQ(ec, error_code());

   // Response to select contains an error
   p.reset();
   done = resp3::parse(p, "!3\r\nBad\r\n", adapter, ec);
   BOOST_TEST(done);
   BOOST_TEST_EQ(ec, error::resp3_hello);
   BOOST_TEST_EQ(st.diagnostic, "Bad");
}

// A NULL is not an error
void test_null()
{
   // Setup
   connection_state st;
   st.cfg.use_setup = true;
   compose_setup_request(st.cfg, st.tracker, st.setup_req);
   setup_adapter adapter{st};

   // Response to HELLO
   resp3::parser p;
   error_code ec;
   bool done = resp3::parse(p, "_\r\n", adapter, ec);
   BOOST_TEST(done);
   BOOST_TEST_EQ(ec, error_code());

   // No diagnostic
   BOOST_TEST_EQ(st.diagnostic, "");
}

// Sentinel adds a ROLE command and checks its output.
// These are real wire values.
constexpr std::string_view role_master_response =
   "*3\r\n$6\r\nmaster\r\n:567942\r\n*2\r\n"
   "*3\r\n$9\r\nlocalhost\r\n$4\r\n6381\r\n$6\r\n567809\r\n*3\r\n$9\r\nlocalhost\r\n"
   "$4\r\n6382\r\n$6\r\n567809\r\n";
constexpr std::string_view role_replica_response =
   "*5\r\n$5\r\nslave\r\n$9\r\nlocalhost\r\n:6380\r\n$9\r\nconnected\r\n:617355\r\n";

void test_sentinel_master()
{
   // Setup
   connection_state st;
   st.cfg.use_setup = true;
   st.cfg.setup.push("SELECT", 2);
   st.cfg.sentinel.addresses = {
      {"localhost", "26379"}
   };
   compose_setup_request(st.cfg, st.tracker, st.setup_req);
   setup_adapter adapter{st};

   // Response to HELLO
   resp3::parser p;
   error_code ec;
   bool done = resp3::parse(p, "%1\r\n$6\r\nserver\r\n$5\r\nredis\r\n", adapter, ec);
   BOOST_TEST(done);
   BOOST_TEST_EQ(ec, error_code());

   // Response to the SELECT command
   p.reset();
   done = resp3::parse(p, "+OK\r\n", adapter, ec);
   BOOST_TEST(done);
   BOOST_TEST_EQ(ec, error_code());

   // Response to the ROLE command
   p.reset();
   done = resp3::parse(p, role_master_response, adapter, ec);
   BOOST_TEST(done);
   BOOST_TEST_EQ(ec, error_code());

   // No diagnostic
   BOOST_TEST_EQ(st.diagnostic, "");
}

void test_sentinel_replica()
{
   // Setup
   connection_state st;
   st.cfg.use_setup = true;
   st.cfg.sentinel.addresses = {
      {"localhost", "26379"}
   };
   st.cfg.sentinel.server_role = role::replica;
   compose_setup_request(st.cfg, st.tracker, st.setup_req);
   setup_adapter adapter{st};

   // Response to HELLO
   resp3::parser p;
   error_code ec;
   bool done = resp3::parse(p, "%1\r\n$6\r\nserver\r\n$5\r\nredis\r\n", adapter, ec);
   BOOST_TEST(done);
   BOOST_TEST_EQ(ec, error_code());

   // Response to the ROLE command
   p.reset();
   done = resp3::parse(p, role_replica_response, adapter, ec);
   BOOST_TEST(done);
   BOOST_TEST_EQ(ec, error_code());

   // No diagnostic
   BOOST_TEST_EQ(st.diagnostic, "");
}

// If the role is not the one expected, a role failed error is issued
void test_sentinel_role_check_failed_master()
{
   // Setup
   connection_state st;
   st.cfg.use_setup = true;
   st.cfg.sentinel.addresses = {
      {"localhost", "26379"}
   };
   compose_setup_request(st.cfg, st.tracker, st.setup_req);
   setup_adapter adapter{st};

   // Response to HELLO
   resp3::parser p;
   error_code ec;
   bool done = resp3::parse(p, "%1\r\n$6\r\nserver\r\n$5\r\nredis\r\n", adapter, ec);
   BOOST_TEST(done);
   BOOST_TEST_EQ(ec, error_code());

   // Response to the ROLE command
   p.reset();
   done = resp3::parse(p, role_replica_response, adapter, ec);
   BOOST_TEST(done);
   BOOST_TEST_EQ(ec, error::role_check_failed);

   // No diagnostic
   BOOST_TEST_EQ(st.diagnostic, "");
}

void test_sentinel_role_check_failed_replica()
{
   // Setup
   connection_state st;
   st.cfg.use_setup = true;
   st.cfg.sentinel.addresses = {
      {"localhost", "26379"}
   };
   st.cfg.sentinel.server_role = role::replica;
   compose_setup_request(st.cfg, st.tracker, st.setup_req);
   setup_adapter adapter{st};

   // Response to HELLO
   resp3::parser p;
   error_code ec;
   bool done = resp3::parse(p, "%1\r\n$6\r\nserver\r\n$5\r\nredis\r\n", adapter, ec);
   BOOST_TEST(done);
   BOOST_TEST_EQ(ec, error_code());

   // Response to the ROLE command
   p.reset();
   done = resp3::parse(p, role_master_response, adapter, ec);
   BOOST_TEST(done);
   BOOST_TEST_EQ(ec, error::role_check_failed);

   // No diagnostic
   BOOST_TEST_EQ(st.diagnostic, "");
}

// If the role command errors or has an unexpected format, we fail
void test_sentinel_role_error_node()
{
   // Setup
   connection_state st;
   st.cfg.use_setup = true;
   st.cfg.setup.clear();
   st.cfg.sentinel.addresses = {
      {"localhost", "26379"}
   };
   compose_setup_request(st.cfg, st.tracker, st.setup_req);
   setup_adapter adapter{st};

   // Response to ROLE
   resp3::parser p;
   error_code ec;
   bool done = resp3::parse(p, "-ERR unauthorized\r\n", adapter, ec);
   BOOST_TEST(done);
   BOOST_TEST_EQ(ec, error::resp3_hello);
   BOOST_TEST_EQ(st.diagnostic, "ERR unauthorized");
}

void test_sentinel_role_not_array()
{
   // Setup
   connection_state st;
   st.cfg.use_setup = true;
   st.cfg.setup.clear();
   st.cfg.sentinel.addresses = {
      {"localhost", "26379"}
   };
   compose_setup_request(st.cfg, st.tracker, st.setup_req);
   setup_adapter adapter{st};

   // Response to ROLE
   resp3::parser p;
   error_code ec;
   bool done = resp3::parse(p, "+OK\r\n", adapter, ec);
   BOOST_TEST(done);
   BOOST_TEST_EQ(ec, error::invalid_data_type);
   BOOST_TEST_EQ(st.diagnostic, "");
}

void test_sentinel_role_empty_array()
{
   // Setup
   connection_state st;
   st.cfg.use_setup = true;
   st.cfg.setup.clear();
   st.cfg.sentinel.addresses = {
      {"localhost", "26379"}
   };
   compose_setup_request(st.cfg, st.tracker, st.setup_req);
   setup_adapter adapter{st};

   // Response to ROLE
   resp3::parser p;
   error_code ec;
   bool done = resp3::parse(p, "*0\r\n", adapter, ec);
   BOOST_TEST(done);
   BOOST_TEST_EQ(ec, error::incompatible_size);
   BOOST_TEST_EQ(st.diagnostic, "");
}

void test_sentinel_role_first_element_not_string()
{
   // Setup
   connection_state st;
   st.cfg.use_setup = true;
   st.cfg.setup.clear();
   st.cfg.sentinel.addresses = {
      {"localhost", "26379"}
   };
   compose_setup_request(st.cfg, st.tracker, st.setup_req);
   setup_adapter adapter{st};

   // Response to ROLE
   resp3::parser p;
   error_code ec;
   bool done = resp3::parse(p, "*1\r\n:2000\r\n", adapter, ec);
   BOOST_TEST(done);
   BOOST_TEST_EQ(ec, error::invalid_data_type);
   BOOST_TEST_EQ(st.diagnostic, "");
}

}  // namespace

int main()
{
   test_success();
   test_simple_error();
   test_blob_error();
   test_null();

   test_sentinel_master();
   test_sentinel_replica();
   test_sentinel_role_check_failed_master();
   test_sentinel_role_check_failed_replica();
   test_sentinel_role_error_node();
   test_sentinel_role_not_array();
   test_sentinel_role_empty_array();
   test_sentinel_role_first_element_not_string();

   return boost::report_errors();
}