/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/adapter/adapt.hpp>
#include <boost/redis/adapter/any_adapter.hpp>
#include <boost/redis/detail/multiplexer.hpp>
#include <boost/redis/detail/resp3_handshaker.hpp>
#include <boost/redis/resp3/node.hpp>
#include <boost/redis/resp3/serialization.hpp>
#include <boost/redis/resp3/type.hpp>
#define BOOST_TEST_MODULE conn_quit
#include <boost/test/included/unit_test.hpp>

#include <iostream>
#include <string>

using boost::redis::request;
using boost::redis::config;
using boost::redis::detail::push_hello;
using boost::redis::response;
using boost::redis::adapter::adapt2;
using boost::redis::adapter::result;
using boost::redis::resp3::detail::deserialize;
using boost::redis::ignore_t;
using boost::redis::detail::multiplexer;
using boost::redis::generic_response;
using boost::redis::resp3::node;
using boost::redis::resp3::to_string;
using boost::redis::any_adapter;

BOOST_AUTO_TEST_CASE(low_level_sync_sans_io)
{
   try {
      result<std::set<std::string>> resp;

      char const* wire = "~6\r\n+orange\r\n+apple\r\n+one\r\n+two\r\n+three\r\n+orange\r\n";
      deserialize(wire, adapt2(resp));

      for (auto const& e : resp.value())
         std::cout << e << std::endl;

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
      exit(EXIT_FAILURE);
   }
}

BOOST_AUTO_TEST_CASE(config_to_hello)
{
   config cfg;
   cfg.clientname = "";
   request req;

   push_hello(cfg, req);

   std::string_view const expected = "*2\r\n$5\r\nHELLO\r\n$1\r\n3\r\n";
   BOOST_CHECK_EQUAL(req.payload(), expected);
}

BOOST_AUTO_TEST_CASE(config_to_hello_with_select)
{
   config cfg;
   cfg.clientname = "";
   cfg.database_index = 10;
   request req;

   push_hello(cfg, req);

   std::string_view const expected =
      "*2\r\n$5\r\nHELLO\r\n$1\r\n3\r\n"
      "*2\r\n$6\r\nSELECT\r\n$2\r\n10\r\n";

   BOOST_CHECK_EQUAL(req.payload(), expected);
}

BOOST_AUTO_TEST_CASE(config_to_hello_cmd_clientname)
{
   config cfg;
   request req;

   push_hello(cfg, req);

   std::string_view const
      expected = "*4\r\n$5\r\nHELLO\r\n$1\r\n3\r\n$7\r\nSETNAME\r\n$11\r\nBoost.Redis\r\n";
   BOOST_CHECK_EQUAL(req.payload(), expected);
}

BOOST_AUTO_TEST_CASE(config_to_hello_cmd_auth)
{
   config cfg;
   cfg.clientname = "";
   cfg.username = "foo";
   cfg.password = "bar";
   request req;

   push_hello(cfg, req);

   std::string_view const
      expected = "*5\r\n$5\r\nHELLO\r\n$1\r\n3\r\n$4\r\nAUTH\r\n$3\r\nfoo\r\n$3\r\nbar\r\n";
   BOOST_CHECK_EQUAL(req.payload(), expected);
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

//===========================================================================
// Multiplexer

std::ostream& operator<<(std::ostream& os, node const& nd)
{
   os << to_string(nd.data_type) << "\n"
      << nd.aggregate_size << "\n"
      << nd.depth << "\n"
      << nd.value;

   return os;
}

BOOST_AUTO_TEST_CASE(multiplexer_push)
{
   multiplexer mpx;
   generic_response resp;
   mpx.set_receive_response(resp);

   mpx.get_read_buffer() = ">2\r\n+one\r\n+two\r\n";

   boost::system::error_code ec;
   auto const ret = mpx.consume_next(ec);

   BOOST_TEST(ret.first.value());
   BOOST_CHECK_EQUAL(ret.second, 16u);

   // TODO: Provide operator << for generic_response so we can compare
   // the whole vector.
   BOOST_CHECK_EQUAL(resp.value().size(), 3u);
   BOOST_CHECK_EQUAL(resp.value().at(1).value, "one");
   BOOST_CHECK_EQUAL(resp.value().at(2).value, "two");

   for (auto const& e : resp.value())
      std::cout << e << std::endl;
}

BOOST_AUTO_TEST_CASE(multiplexer_push_needs_more)
{
   multiplexer mpx;
   generic_response resp;
   mpx.set_receive_response(resp);

   // Only part of the message.
   mpx.get_read_buffer() = ">2\r\n+one\r";

   boost::system::error_code ec;
   auto ret = mpx.consume_next(ec);

   BOOST_TEST(!ret.first.has_value());

   mpx.get_read_buffer().append("\n+two\r\n");
   ret = mpx.consume_next(ec);

   BOOST_TEST(ret.first.value());
   BOOST_CHECK_EQUAL(ret.second, 16u);

   // TODO: Provide operator << for generic_response so we can compare
   // the whole vector.
   BOOST_CHECK_EQUAL(resp.value().size(), 3u);
   BOOST_CHECK_EQUAL(resp.value().at(1).value, "one");
   BOOST_CHECK_EQUAL(resp.value().at(2).value, "two");
}

struct test_item {
   request req;
   generic_response resp;
   std::shared_ptr<multiplexer::elem> elem_ptr;
   bool done = false;

   test_item(bool cmd_with_response = true)
   {
      // The exact command is irrelevant because it is not being sent
      // to Redis.
      req.push(cmd_with_response ? "PING" : "SUBSCRIBE", "cmd-arg");

      elem_ptr = std::make_shared<multiplexer::elem>(req, any_adapter(resp).impl_.adapt_fn);

      elem_ptr->set_done_callback([this]() {
         done = true;
      });
   }
};

BOOST_AUTO_TEST_CASE(multiplexer_pipeline)
{
   test_item item1{};
   test_item item2{false};
   test_item item3{};

   // Add some requests to the multiplexer.
   multiplexer mpx;
   mpx.add(item1.elem_ptr);
   mpx.add(item3.elem_ptr);
   mpx.add(item2.elem_ptr);

   // These requests haven't been written yet so their statuses should
   // be "waiting.".
   BOOST_TEST(item1.elem_ptr->is_waiting());
   BOOST_TEST(item2.elem_ptr->is_waiting());
   BOOST_TEST(item3.elem_ptr->is_waiting());

   // There are three requests to coalesce, a second call should do
   // nothing.
   BOOST_CHECK_EQUAL(mpx.prepare_write(), 3u);
   BOOST_CHECK_EQUAL(mpx.prepare_write(), 0u);

   // After coalescing the requests for writing their statuses should
   // be changed to "staged".
   BOOST_TEST(item1.elem_ptr->is_staged());
   BOOST_TEST(item2.elem_ptr->is_staged());
   BOOST_TEST(item3.elem_ptr->is_staged());

   // There are no waiting requests to cancel since they are all
   // staged.
   BOOST_CHECK_EQUAL(mpx.cancel_waiting(), 0u);

   // Since the requests haven't been sent (written) the done
   // callback should not have been called yet.
   BOOST_TEST(!item1.done);
   BOOST_TEST(!item2.done);
   BOOST_TEST(!item3.done);

   // The commit_write call informs the multiplexer the payload was
   // sent (e.g.  written to the socket). This step releases requests
   // that has no response.
   BOOST_CHECK_EQUAL(mpx.commit_write(), 1u);

   // The staged status should now have changed to written.
   BOOST_TEST(item1.elem_ptr->is_written());
   BOOST_TEST(item2.elem_ptr->is_done());
   BOOST_TEST(item3.elem_ptr->is_written());

   // The done status should still be unchanged on requests that
   // expect a response.
   BOOST_TEST(!item1.done);
   BOOST_TEST(item2.done);
   BOOST_TEST(!item3.done);

   // Simulates a socket read by putting some data in the read buffer.
   mpx.get_read_buffer().append("+one\r\n");

   // Consumes the next message in the read buffer.
   boost::system::error_code ec;
   auto const ret = mpx.consume_next(ec);

   // The read operation should have been successfull.
   BOOST_TEST(ret.first.has_value());
   BOOST_TEST(ret.second != 0u);

   // The read buffer should also be empty now
   BOOST_TEST(mpx.get_read_buffer().empty());

   // The last request still did not get a response.
   BOOST_TEST(item1.done);
   BOOST_TEST(item2.done);
   BOOST_TEST(!item3.done);

   // TODO: Check the first request was removed from the queue.
}
