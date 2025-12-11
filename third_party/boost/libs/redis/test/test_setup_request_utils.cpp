//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/adapter/result.hpp>
#include <boost/redis/config.hpp>
#include <boost/redis/error.hpp>
#include <boost/redis/impl/setup_request_utils.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/resp3/type.hpp>
#include <boost/redis/response.hpp>

#include <boost/asio/error.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/system/result.hpp>

namespace asio = boost::asio;
namespace redis = boost::redis;
using redis::detail::compose_setup_request;
using boost::system::error_code;

namespace {

void test_compose_setup()
{
   redis::config cfg;
   cfg.clientname = "";

   compose_setup_request(cfg);

   std::string_view const expected = "*2\r\n$5\r\nHELLO\r\n$1\r\n3\r\n";
   BOOST_TEST_EQ(cfg.setup.payload(), expected);
   BOOST_TEST(cfg.setup.has_hello_priority());
   BOOST_TEST(cfg.setup.get_config().cancel_if_unresponded);
   BOOST_TEST(cfg.setup.get_config().cancel_on_connection_lost);
}

void test_compose_setup_select()
{
   redis::config cfg;
   cfg.clientname = "";
   cfg.database_index = 10;

   compose_setup_request(cfg);

   std::string_view const expected =
      "*2\r\n$5\r\nHELLO\r\n$1\r\n3\r\n"
      "*2\r\n$6\r\nSELECT\r\n$2\r\n10\r\n";
   BOOST_TEST_EQ(cfg.setup.payload(), expected);
   BOOST_TEST(cfg.setup.has_hello_priority());
   BOOST_TEST(cfg.setup.get_config().cancel_if_unresponded);
   BOOST_TEST(cfg.setup.get_config().cancel_on_connection_lost);
}

void test_compose_setup_clientname()
{
   redis::config cfg;

   compose_setup_request(cfg);

   std::string_view const
      expected = "*4\r\n$5\r\nHELLO\r\n$1\r\n3\r\n$7\r\nSETNAME\r\n$11\r\nBoost.Redis\r\n";
   BOOST_TEST_EQ(cfg.setup.payload(), expected);
   BOOST_TEST(cfg.setup.has_hello_priority());
   BOOST_TEST(cfg.setup.get_config().cancel_if_unresponded);
   BOOST_TEST(cfg.setup.get_config().cancel_on_connection_lost);
}

void test_compose_setup_auth()
{
   redis::config cfg;
   cfg.clientname = "";
   cfg.username = "foo";
   cfg.password = "bar";

   compose_setup_request(cfg);

   std::string_view const
      expected = "*5\r\n$5\r\nHELLO\r\n$1\r\n3\r\n$4\r\nAUTH\r\n$3\r\nfoo\r\n$3\r\nbar\r\n";
   BOOST_TEST_EQ(cfg.setup.payload(), expected);
   BOOST_TEST(cfg.setup.has_hello_priority());
   BOOST_TEST(cfg.setup.get_config().cancel_if_unresponded);
   BOOST_TEST(cfg.setup.get_config().cancel_on_connection_lost);
}

void test_compose_setup_auth_empty_password()
{
   redis::config cfg;
   cfg.clientname = "";
   cfg.username = "foo";

   compose_setup_request(cfg);

   std::string_view const
      expected = "*5\r\n$5\r\nHELLO\r\n$1\r\n3\r\n$4\r\nAUTH\r\n$3\r\nfoo\r\n$0\r\n\r\n";
   BOOST_TEST_EQ(cfg.setup.payload(), expected);
   BOOST_TEST(cfg.setup.has_hello_priority());
   BOOST_TEST(cfg.setup.get_config().cancel_if_unresponded);
   BOOST_TEST(cfg.setup.get_config().cancel_on_connection_lost);
}

void test_compose_setup_auth_setname()
{
   redis::config cfg;
   cfg.clientname = "mytest";
   cfg.username = "foo";
   cfg.password = "bar";

   compose_setup_request(cfg);

   std::string_view const expected =
      "*7\r\n$5\r\nHELLO\r\n$1\r\n3\r\n$4\r\nAUTH\r\n$3\r\nfoo\r\n$3\r\nbar\r\n$7\r\nSETNAME\r\n$"
      "6\r\nmytest\r\n";
   BOOST_TEST_EQ(cfg.setup.payload(), expected);
   BOOST_TEST(cfg.setup.has_hello_priority());
   BOOST_TEST(cfg.setup.get_config().cancel_if_unresponded);
   BOOST_TEST(cfg.setup.get_config().cancel_on_connection_lost);
}

void test_compose_setup_use_setup()
{
   redis::config cfg;
   cfg.clientname = "mytest";
   cfg.username = "foo";
   cfg.password = "bar";
   cfg.database_index = 4;
   cfg.use_setup = true;
   cfg.setup.push("SELECT", 8);

   compose_setup_request(cfg);

   std::string_view const expected =
      "*2\r\n$5\r\nHELLO\r\n$1\r\n3\r\n"
      "*2\r\n$6\r\nSELECT\r\n$1\r\n8\r\n";
   BOOST_TEST_EQ(cfg.setup.payload(), expected);
   BOOST_TEST(cfg.setup.has_hello_priority());
   BOOST_TEST(cfg.setup.get_config().cancel_if_unresponded);
   BOOST_TEST(cfg.setup.get_config().cancel_on_connection_lost);
}

// Regression check: we set the priority flag
void test_compose_setup_use_setup_no_hello()
{
   redis::config cfg;
   cfg.use_setup = true;
   cfg.setup.clear();
   cfg.setup.push("SELECT", 8);

   compose_setup_request(cfg);

   std::string_view const expected = "*2\r\n$6\r\nSELECT\r\n$1\r\n8\r\n";
   BOOST_TEST_EQ(cfg.setup.payload(), expected);
   BOOST_TEST(cfg.setup.has_hello_priority());
   BOOST_TEST(cfg.setup.get_config().cancel_if_unresponded);
   BOOST_TEST(cfg.setup.get_config().cancel_on_connection_lost);
}

// Regression check: we set the relevant cancellation flags in the request
void test_compose_setup_use_setup_flags()
{
   redis::config cfg;
   cfg.use_setup = true;
   cfg.setup.clear();
   cfg.setup.push("SELECT", 8);
   cfg.setup.get_config().cancel_if_unresponded = false;
   cfg.setup.get_config().cancel_on_connection_lost = false;

   compose_setup_request(cfg);

   std::string_view const expected = "*2\r\n$6\r\nSELECT\r\n$1\r\n8\r\n";
   BOOST_TEST_EQ(cfg.setup.payload(), expected);
   BOOST_TEST(cfg.setup.has_hello_priority());
   BOOST_TEST(cfg.setup.get_config().cancel_if_unresponded);
   BOOST_TEST(cfg.setup.get_config().cancel_on_connection_lost);
}

}  // namespace

int main()
{
   test_compose_setup();
   test_compose_setup_select();
   test_compose_setup_clientname();
   test_compose_setup_auth();
   test_compose_setup_auth_empty_password();
   test_compose_setup_auth_setname();
   test_compose_setup_use_setup();
   test_compose_setup_use_setup_no_hello();
   test_compose_setup_use_setup_flags();

   return boost::report_errors();
}