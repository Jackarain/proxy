/* Copyright (c) 2018-2026 Marcelo Zimbres Silva (mzimbres@gmail.com),
 * Ruben Perez Hidalgo (rubenperez038@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/adapter/adapt.hpp>
#include <boost/redis/resp3/node.hpp>
#include <boost/redis/resp3/serialization.hpp>
#include <boost/redis/resp3/type.hpp>
#include <boost/redis/response.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/system/error_code.hpp>

#include "print_node.hpp"

using namespace boost::redis;
using boost::system::error_code;
using boost::redis::resp3::detail::deserialize;
using resp3::node_view;
using resp3::type;
using adapter::adapt2;

// Regular nodes are just stored
void test_success()
{
   generic_flat_response resp;

   error_code ec;
   deserialize("+hello\r\n", adapt2(resp), ec);
   BOOST_TEST_EQ(ec, error_code{});

   BOOST_TEST(resp.has_value());
   std::vector<node_view> expected_nodes{
      {type::simple_string, 1u, 0u, "hello"},
   };
   BOOST_TEST_ALL_EQ(resp->begin(), resp->end(), expected_nodes.begin(), expected_nodes.end());
}

// If an error of any kind appears, we set the overall result to error
void test_simple_error()
{
   generic_flat_response resp;

   error_code ec;
   deserialize("-Error\r\n", adapt2(resp), ec);
   BOOST_TEST_EQ(ec, error_code{});

   BOOST_TEST(resp.has_error());
   auto const error = resp.error();

   BOOST_TEST_EQ(error.data_type, resp3::type::simple_error);
   BOOST_TEST_EQ(error.diagnostic, "Error");
}

void test_blob_error()
{
   generic_flat_response resp;

   error_code ec;
   deserialize("!5\r\nError\r\n", adapt2(resp), ec);
   BOOST_TEST_EQ(ec, error_code{});

   BOOST_TEST(resp.has_error());
   auto const error = resp.error();

   BOOST_TEST_EQ(error.data_type, resp3::type::blob_error);
   BOOST_TEST_EQ(error.diagnostic, "Error");
}

// Mixing success and error nodes is safe. Only the last error is stored
void test_mix_success_error()
{
   generic_flat_response resp;
   error_code ec;

   // Success message
   deserialize("+message\r\n", adapt2(resp), ec);
   BOOST_TEST_EQ(ec, error_code{});

   // An error
   deserialize("-Error\r\n", adapt2(resp), ec);
   BOOST_TEST_EQ(ec, error_code{});

   // Another success message
   deserialize("+other data\r\n", adapt2(resp), ec);
   BOOST_TEST_EQ(ec, error_code{});

   // Another error
   deserialize("-Different err\r\n", adapt2(resp), ec);
   BOOST_TEST_EQ(ec, error_code{});

   // Final success message
   deserialize("*1\r\n+last message\r\n", adapt2(resp), ec);
   BOOST_TEST_EQ(ec, error_code{});

   // Check
   BOOST_TEST(resp.has_error());
   auto const error = resp.error();

   BOOST_TEST_EQ(error.data_type, resp3::type::simple_error);
   BOOST_TEST_EQ(error.diagnostic, "Different err");
}

int main()
{
   test_success();
   test_simple_error();
   test_blob_error();
   test_mix_success_error();

   return boost::report_errors();
}
