/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/adapter/adapt.hpp>
#include <boost/redis/adapter/any_adapter.hpp>
#include <boost/redis/detail/read_buffer.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/resp3/node.hpp>
#include <boost/redis/resp3/serialization.hpp>
#include <boost/redis/resp3/type.hpp>
#include <boost/redis/response.hpp>

#define BOOST_TEST_MODULE low_level_sync_sans_io
#include <boost/test/included/unit_test.hpp>

#include <iostream>
#include <string>

using boost::redis::request;
using boost::redis::adapter::adapt2;
using boost::redis::adapter::result;
using boost::redis::generic_response;
using boost::redis::ignore_t;
using boost::redis::resp3::detail::deserialize;
using boost::redis::resp3::node;
using boost::redis::resp3::to_string;
using boost::redis::response;
using boost::redis::any_adapter;
using boost::system::error_code;

#define RESP3_SET_PART1 "~6\r\n+orange\r"
#define RESP3_SET_PART2 "\n+apple\r\n+one"
#define RESP3_SET_PART3 "\r\n+two\r"
#define RESP3_SET_PART4 "\n+three\r\n+orange\r\n"
char const* resp3_set = RESP3_SET_PART1 RESP3_SET_PART2 RESP3_SET_PART3 RESP3_SET_PART4;

BOOST_AUTO_TEST_CASE(low_level_sync_sans_io)
{
   try {
      result<std::set<std::string>> resp;

      deserialize(resp3_set, adapt2(resp));

      for (auto const& e : resp.value())
         std::cout << e << std::endl;

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
      exit(EXIT_FAILURE);
   }
}

BOOST_AUTO_TEST_CASE(issue_210_empty_set)
{
   try {
      result<std::tuple<
         result<int>,
         result<std::vector<std::string>>,
         result<std::string>,
         result<int>>>
         resp;

      char const* wire = "*4\r\n:1\r\n~0\r\n$25\r\nthis_should_not_be_in_set\r\n:2\r\n";

      deserialize(wire, adapt2(resp));

      BOOST_CHECK_EQUAL(std::get<0>(resp.value()).value(), 1);
      BOOST_CHECK(std::get<1>(resp.value()).value().empty());
      BOOST_CHECK_EQUAL(std::get<2>(resp.value()).value(), "this_should_not_be_in_set");
      BOOST_CHECK_EQUAL(std::get<3>(resp.value()).value(), 2);

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
      exit(EXIT_FAILURE);
   }
}

BOOST_AUTO_TEST_CASE(issue_210_non_empty_set_size_one)
{
   try {
      result<std::tuple<
         result<int>,
         result<std::vector<std::string>>,
         result<std::string>,
         result<int>>>
         resp;

      char const*
         wire = "*4\r\n:1\r\n~1\r\n$3\r\nfoo\r\n$25\r\nthis_should_not_be_in_set\r\n:2\r\n";

      deserialize(wire, adapt2(resp));

      BOOST_CHECK_EQUAL(std::get<0>(resp.value()).value(), 1);
      BOOST_CHECK_EQUAL(std::get<1>(resp.value()).value().size(), 1u);
      BOOST_CHECK_EQUAL(std::get<1>(resp.value()).value().at(0), std::string{"foo"});
      BOOST_CHECK_EQUAL(std::get<2>(resp.value()).value(), "this_should_not_be_in_set");
      BOOST_CHECK_EQUAL(std::get<3>(resp.value()).value(), 2);

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
      exit(EXIT_FAILURE);
   }
}

BOOST_AUTO_TEST_CASE(issue_210_non_empty_set_size_two)
{
   try {
      result<std::tuple<
         result<int>,
         result<std::vector<std::string>>,
         result<std::string>,
         result<int>>>
         resp;

      char const* wire =
         "*4\r\n:1\r\n~2\r\n$3\r\nfoo\r\n$3\r\nbar\r\n$25\r\nthis_should_not_be_in_set\r\n:2\r\n";

      deserialize(wire, adapt2(resp));

      BOOST_CHECK_EQUAL(std::get<0>(resp.value()).value(), 1);
      BOOST_CHECK_EQUAL(std::get<1>(resp.value()).value().at(0), std::string{"foo"});
      BOOST_CHECK_EQUAL(std::get<1>(resp.value()).value().at(1), std::string{"bar"});
      BOOST_CHECK_EQUAL(std::get<2>(resp.value()).value(), "this_should_not_be_in_set");

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
      exit(EXIT_FAILURE);
   }
}

BOOST_AUTO_TEST_CASE(issue_210_no_nested)
{
   try {
      result<std::tuple<result<int>, result<std::string>, result<std::string>, result<std::string>>>
         resp;

      char const*
         wire = "*4\r\n:1\r\n$3\r\nfoo\r\n$3\r\nbar\r\n$25\r\nthis_should_not_be_in_set\r\n";

      deserialize(wire, adapt2(resp));

      BOOST_CHECK_EQUAL(std::get<0>(resp.value()).value(), 1);
      BOOST_CHECK_EQUAL(std::get<1>(resp.value()).value(), std::string{"foo"});
      BOOST_CHECK_EQUAL(std::get<2>(resp.value()).value(), std::string{"bar"});
      BOOST_CHECK_EQUAL(std::get<3>(resp.value()).value(), "this_should_not_be_in_set");

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
      exit(EXIT_FAILURE);
   }
}

BOOST_AUTO_TEST_CASE(issue_233_array_with_null)
{
   try {
      result<std::vector<std::optional<std::string>>> resp;

      char const* wire = "*3\r\n+one\r\n_\r\n+two\r\n";
      deserialize(wire, adapt2(resp));

      BOOST_CHECK_EQUAL(resp.value().at(0).value(), "one");
      BOOST_TEST(!resp.value().at(1).has_value());
      BOOST_CHECK_EQUAL(resp.value().at(2).value(), "two");

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
      exit(EXIT_FAILURE);
   }
}

BOOST_AUTO_TEST_CASE(issue_233_optional_array_with_null)
{
   try {
      result<std::optional<std::vector<std::optional<std::string>>>> resp;

      char const* wire = "*3\r\n+one\r\n_\r\n+two\r\n";
      deserialize(wire, adapt2(resp));

      BOOST_CHECK_EQUAL(resp.value().value().at(0).value(), "one");
      BOOST_TEST(!resp.value().value().at(1).has_value());
      BOOST_CHECK_EQUAL(resp.value().value().at(2).value(), "two");

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
      exit(EXIT_FAILURE);
   }
}

BOOST_AUTO_TEST_CASE(read_buffer_prepare_error)
{
   using boost::redis::detail::read_buffer;

   read_buffer buf;

   // Usual case, max size is bigger then requested size.
   buf.set_config({10, 10});
   auto ec = buf.prepare();
   BOOST_TEST(!ec);
   buf.commit(10);

   // Corner case, max size is equal to the requested size.
   buf.set_config({10, 20});
   ec = buf.prepare();
   BOOST_TEST(!ec);
   buf.commit(10);
   buf.consume(20);

   auto const tmp = buf;

   // Error case, max size is smaller to the requested size.
   buf.set_config({10, 9});
   ec = buf.prepare();
   BOOST_TEST(ec == error_code{boost::redis::error::exceeds_maximum_read_buffer_size});

   // Check that an error call has no side effects.
   auto const res = buf == tmp;
   BOOST_TEST(res);
}

BOOST_AUTO_TEST_CASE(read_buffer_prepare_consume_only_committed_data)
{
   using boost::redis::detail::read_buffer;

   read_buffer buf;

   buf.set_config({10, 10});
   auto ec = buf.prepare();
   BOOST_TEST(!ec);

   auto res = buf.consume(5);

   // No data has been committed yet so nothing can be consummed.
   BOOST_CHECK_EQUAL(res.consumed, 0u);

   // If nothing was consumed, nothing got rotated.
   BOOST_CHECK_EQUAL(res.rotated, 0u);

   buf.commit(10);
   res = buf.consume(5);

   // All five bytes should have been consumed.
   BOOST_CHECK_EQUAL(res.consumed, 5u);

   // We added a total of 10 bytes and consumed 5, that means, 5 were
   // rotated.
   BOOST_CHECK_EQUAL(res.rotated, 5u);

   res = buf.consume(7);

   // Only the remaining five bytes can be consumed
   BOOST_CHECK_EQUAL(res.consumed, 5u);

   // No bytes to rotated.
   BOOST_CHECK_EQUAL(res.rotated, 0u);
}

BOOST_AUTO_TEST_CASE(read_buffer_check_buffer_size)
{
   using boost::redis::detail::read_buffer;

   read_buffer buf;

   buf.set_config({10, 10});
   auto ec = buf.prepare();
   BOOST_TEST(!ec);

   BOOST_CHECK_EQUAL(buf.get_prepared().size(), 10u);
}

BOOST_AUTO_TEST_CASE(check_counter_adapter)
{
   using boost::redis::any_adapter;
   using boost::redis::resp3::parse;
   using boost::redis::resp3::parser;
   using boost::redis::resp3::node_view;
   using boost::system::error_code;

   int init = 0;
   int node = 0;
   int done = 0;

   auto counter_adapter = [&](any_adapter::parse_event ev, node_view const&, error_code&) mutable {
      switch (ev) {
         case any_adapter::parse_event::init: init++; break;
         case any_adapter::parse_event::node: node++; break;
         case any_adapter::parse_event::done: done++; break;
      }
   };

   any_adapter wrapped{any_adapter::impl_t{counter_adapter}};

   error_code ec;
   parser p;

   auto const ret1 = parse(p, RESP3_SET_PART1, wrapped, ec);
   auto const ret2 = parse(p, RESP3_SET_PART1 RESP3_SET_PART2, wrapped, ec);
   auto const ret3 = parse(p, RESP3_SET_PART1 RESP3_SET_PART2 RESP3_SET_PART3, wrapped, ec);
   auto const ret4 = parse(
      p,
      RESP3_SET_PART1 RESP3_SET_PART2 RESP3_SET_PART3 RESP3_SET_PART4,
      wrapped,
      ec);

   BOOST_TEST(!ret1);
   BOOST_TEST(!ret2);
   BOOST_TEST(!ret3);
   BOOST_TEST(ret4);

   BOOST_CHECK_EQUAL(init, 1);
   BOOST_CHECK_EQUAL(node, 7);
   BOOST_CHECK_EQUAL(done, 1);
}
