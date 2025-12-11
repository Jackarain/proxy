/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/request.hpp>

#include <boost/core/lightweight_test.hpp>

#include <map>
#include <string>
#include <string_view>

using boost::redis::request;

// TODO: Serialization.

namespace {

void test_push_no_args()
{
   request req1;
   req1.push("PING");
   BOOST_TEST_EQ(req1.payload(), "*1\r\n$4\r\nPING\r\n");
}

void test_push_int()
{
   request req;
   req.push("PING", 42);
   BOOST_TEST_EQ(req.payload(), "*2\r\n$4\r\nPING\r\n$2\r\n42\r\n");
}

void test_push_multiple_args()
{
   char const* res = "*5\r\n$3\r\nSET\r\n$3\r\nkey\r\n$5\r\nvalue\r\n$2\r\nEX\r\n$1\r\n2\r\n";
   request req;
   req.push("SET", "key", "value", "EX", "2");
   BOOST_TEST_EQ(req.payload(), res);
}

void test_push_range()
{
   std::map<std::string, std::string> in{
      {"key1", "value1"},
      {"key2", "value2"}
   };

   constexpr std::string_view expected =
      "*6\r\n$4\r\nHSET\r\n$3\r\nkey\r\n$4\r\nkey1\r\n$6\r\nvalue1\r\n$4\r\nkey2\r\n$"
      "6\r\nvalue2\r\n";

   request req1;
   req1.push_range("HSET", "key", in);
   BOOST_TEST_EQ(req1.payload(), expected);

   request req2;
   req2.push_range("HSET", "key", std::cbegin(in), std::cend(in));
   BOOST_TEST_EQ(req2.payload(), expected);
}

// Append
void test_append()
{
   request req1;
   req1.push("PING", "req1");

   request req2;
   req2.push("GET", "mykey");
   req2.push("GET", "other");

   req1.append(req2);

   constexpr std::string_view expected =
      "*2\r\n$4\r\nPING\r\n$4\r\nreq1\r\n"
      "*2\r\n$3\r\nGET\r\n$5\r\nmykey\r\n"
      "*2\r\n$3\r\nGET\r\n$5\r\nother\r\n";
   BOOST_TEST_EQ(req1.payload(), expected);
   BOOST_TEST_EQ(req1.get_commands(), 3u);
   BOOST_TEST_EQ(req1.get_expected_responses(), 3u);
}

// Commands without responses are handled correctly
void test_append_no_response()
{
   request req1;
   req1.push("PING", "req1");

   request req2;
   req2.push("SUBSCRIBE", "mychannel");
   req2.push("GET", "other");

   req1.append(req2);

   constexpr std::string_view expected =
      "*2\r\n$4\r\nPING\r\n$4\r\nreq1\r\n"
      "*2\r\n$9\r\nSUBSCRIBE\r\n$9\r\nmychannel\r\n"
      "*2\r\n$3\r\nGET\r\n$5\r\nother\r\n";
   BOOST_TEST_EQ(req1.payload(), expected);
   BOOST_TEST_EQ(req1.get_commands(), 3u);
   BOOST_TEST_EQ(req1.get_expected_responses(), 2u);
}

// Flags are not modified by append
void test_append_flags()
{
   request req1;
   req1.get_config().cancel_if_not_connected = false;
   req1.get_config().cancel_if_unresponded = false;
   req1.get_config().cancel_on_connection_lost = false;
   req1.push("PING", "req1");

   request req2;
   req2.get_config().cancel_if_not_connected = true;
   req2.get_config().cancel_if_unresponded = true;
   req2.get_config().cancel_on_connection_lost = true;
   req2.push("GET", "other");

   req1.append(req2);

   constexpr std::string_view expected =
      "*2\r\n$4\r\nPING\r\n$4\r\nreq1\r\n"
      "*2\r\n$3\r\nGET\r\n$5\r\nother\r\n";
   BOOST_TEST_EQ(req1.payload(), expected);
   BOOST_TEST_NOT(req1.get_config().cancel_if_not_connected);
   BOOST_TEST_NOT(req1.get_config().cancel_if_unresponded);
   BOOST_TEST_NOT(req1.get_config().cancel_on_connection_lost);
}

// Empty requests don't cause problems with append
void test_append_target_empty()
{
   request req1;

   request req2;
   req2.push("GET", "other");

   req1.append(req2);

   constexpr std::string_view expected = "*2\r\n$3\r\nGET\r\n$5\r\nother\r\n";
   BOOST_TEST_EQ(req1.payload(), expected);
   BOOST_TEST_EQ(req1.get_commands(), 1u);
   BOOST_TEST_EQ(req1.get_expected_responses(), 1u);
}

void test_append_source_empty()
{
   request req1;
   req1.push("GET", "other");

   request req2;

   req1.append(req2);

   constexpr std::string_view expected = "*2\r\n$3\r\nGET\r\n$5\r\nother\r\n";
   BOOST_TEST_EQ(req1.payload(), expected);
   BOOST_TEST_EQ(req1.get_commands(), 1u);
   BOOST_TEST_EQ(req1.get_expected_responses(), 1u);
}

void test_append_both_empty()
{
   request req1;
   request req2;

   req1.append(req2);

   BOOST_TEST_EQ(req1.payload(), "");
   BOOST_TEST_EQ(req1.get_commands(), 0u);
   BOOST_TEST_EQ(req1.get_expected_responses(), 0u);
}

}  // namespace

int main()
{
   test_push_no_args();
   test_push_int();
   test_push_multiple_args();
   test_push_range();

   test_append();
   test_append_no_response();
   test_append_flags();
   test_append_target_empty();
   test_append_source_empty();
   test_append_both_empty();

   return boost::report_errors();
}
