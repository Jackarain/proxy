/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/adapter/any_adapter.hpp>
#include <boost/redis/detail/multiplexer.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/resp3/node.hpp>
#include <boost/redis/resp3/serialization.hpp>
#include <boost/redis/resp3/type.hpp>
#include <boost/redis/response.hpp>

#include <boost/assert/source_location.hpp>
#include <boost/core/lightweight_test.hpp>

#include "sansio_utils.hpp"

#include <iostream>
#include <memory>
#include <ostream>
#include <string_view>

using boost::redis::request;
using boost::redis::detail::multiplexer;
using boost::redis::detail::consume_result;
using boost::redis::generic_response;
using boost::redis::resp3::node;
using boost::redis::resp3::to_string;
using boost::redis::resp3::type;
using boost::redis::response;
using boost::redis::any_adapter;
using boost::system::error_code;

namespace boost::redis::resp3 {

std::ostream& operator<<(std::ostream& os, node const& nd)
{
   return os << "node{ .data_type=" << to_string(nd.data_type)
             << ", .aggregate_size=" << nd.aggregate_size << ", .depth=" << nd.depth
             << ", .value=" << nd.value << "}";
}

}  // namespace boost::redis::resp3

namespace boost::redis::detail {

std::ostream& operator<<(std::ostream& os, consume_result v)
{
   switch (v) {
      case consume_result::needs_more:   return os << "consume_result::needs_more";
      case consume_result::got_response: return os << "consume_result::got_response";
      case consume_result::got_push:     return os << "consume_result::got_push";
      default:                           return os << "<unknown consume_result>";
   }
}

}  // namespace boost::redis::detail

namespace {

struct test_item {
   request req;
   generic_response resp;
   std::shared_ptr<multiplexer::elem> elem_ptr;
   bool done = false;

   static request make_request(bool cmd_with_response = true)
   {
      request ret;

      // The exact command is irrelevant because it is not being sent
      // to Redis.
      ret.push(cmd_with_response ? "PING" : "SUBSCRIBE", "cmd-arg");

      return ret;
   }

   explicit test_item(request request_value)
   : req{std::move(request_value)}
   {
      elem_ptr = std::make_shared<multiplexer::elem>(req, any_adapter{resp});

      elem_ptr->set_done_callback([this]() {
         done = true;
      });
   }

   test_item(bool cmd_with_response = true)
   : test_item(make_request(cmd_with_response))
   { }
};

void check_response(
   const generic_response& actual,
   boost::span<const node> expected,
   boost::source_location loc = BOOST_CURRENT_LOCATION)
{
   if (!BOOST_TEST(actual.has_value())) {
      std::cerr << "Response has error: " << actual.error().diagnostic << "\n"
                << "Called from " << loc << std::endl;
      return;
   }

   if (!BOOST_TEST_ALL_EQ(actual->begin(), actual->end(), expected.begin(), expected.end())) {
      std::cerr << "Called from " << loc << std::endl;
   }
}

void test_request_needs_more()
{
   // Setup
   test_item item1{};
   multiplexer mpx;

   // Add the element to the multiplexer and simulate a successful write
   mpx.add(item1.elem_ptr);
   BOOST_TEST_EQ(mpx.prepare_write(), 1u);
   BOOST_TEST(mpx.commit_write(item1.req.payload().size()));
   BOOST_TEST(item1.elem_ptr->is_written());
   BOOST_TEST(!item1.done);

   // Parse part of the response
   error_code ec;
   read(mpx, "$11\r\nhello");
   auto ret = mpx.consume(ec);
   BOOST_TEST_EQ(ret.first, consume_result::needs_more);

   // Parse the rest of it
   read(mpx, " world\r\n");
   ret = mpx.consume(ec);
   BOOST_TEST_EQ(ret.first, consume_result::got_response);
   const node expected[] = {
      {type::blob_string, 1u, 0u, "hello world"},
   };
   check_response(item1.resp, expected);
}

void test_several_requests()
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
   BOOST_TEST_EQ(mpx.prepare_write(), 3u);
   BOOST_TEST_EQ(mpx.prepare_write(), 0u);

   // The write buffer holds the 3 requests, coalesced
   constexpr std::string_view expected_buffer =
      "*2\r\n$4\r\nPING\r\n$7\r\ncmd-arg\r\n"
      "*2\r\n$4\r\nPING\r\n$7\r\ncmd-arg\r\n"
      "*2\r\n$9\r\nSUBSCRIBE\r\n$7\r\ncmd-arg\r\n";
   BOOST_TEST_EQ(mpx.get_write_buffer(), expected_buffer);

   // After coalescing the requests for writing their statuses should
   // be changed to "staged".
   BOOST_TEST(item1.elem_ptr->is_staged());
   BOOST_TEST(item2.elem_ptr->is_staged());
   BOOST_TEST(item3.elem_ptr->is_staged());

   // There are no waiting requests to cancel since they are all
   // staged.
   BOOST_TEST_EQ(mpx.cancel_waiting(), 0u);

   // Since the requests haven't been sent (written) the done
   // callback should not have been called yet.
   BOOST_TEST(!item1.done);
   BOOST_TEST(!item2.done);
   BOOST_TEST(!item3.done);

   // The commit_write call informs the multiplexer the payload was
   // sent (e.g.  written to the socket). This step releases requests
   // that has no response.
   BOOST_TEST(mpx.commit_write(expected_buffer.size()));

   // The staged status should now have changed to written.
   BOOST_TEST(item1.elem_ptr->is_written());
   BOOST_TEST(item2.elem_ptr->is_done());
   BOOST_TEST(item3.elem_ptr->is_written());

   // The done status should still be unchanged on requests that
   // expect a response.
   BOOST_TEST(!item1.done);
   BOOST_TEST(item2.done);
   BOOST_TEST(!item3.done);

   // Consumes the next message in the read buffer.
   error_code ec;
   read(mpx, "+one\r\n");
   auto ret = mpx.consume(ec);

   // The read operation should have been successful.
   BOOST_TEST_EQ(ret.first, consume_result::got_response);
   BOOST_TEST(ret.second != 0u);

   // The last request still did not get a response.
   BOOST_TEST(item1.done);
   BOOST_TEST(item2.done);
   BOOST_TEST(!item3.done);

   // Consumes the second message in the read buffer
   // Consumes the next message in the read buffer.
   read(mpx, "+two\r\n");
   ret = mpx.consume(ec);

   // The read operation should have been successful.
   BOOST_TEST_EQ(ret.first, consume_result::got_response);
   BOOST_TEST(ret.second != 0u);

   // Everything done
   BOOST_TEST(item1.done);
   BOOST_TEST(item2.done);
   BOOST_TEST(item3.done);
}

void test_short_writes()
{
   // Setup
   multiplexer mpx;
   test_item item1{};
   test_item item2{false};

   // Add some requests to the multiplexer.
   mpx.add(item1.elem_ptr);
   mpx.add(item2.elem_ptr);
   BOOST_TEST(item1.elem_ptr->is_waiting());
   BOOST_TEST(item2.elem_ptr->is_waiting());

   // Start writing them
   BOOST_TEST_EQ(mpx.prepare_write(), 2u);
   BOOST_TEST_EQ(
      mpx.get_write_buffer(),
      "*2\r\n$4\r\nPING\r\n$7\r\ncmd-arg\r\n"
      "*2\r\n$9\r\nSUBSCRIBE\r\n$7\r\ncmd-arg\r\n");
   BOOST_TEST(item1.elem_ptr->is_staged());
   BOOST_TEST(item2.elem_ptr->is_staged());

   // Write a small part. The write buffer gets updated, but request status is not changed
   BOOST_TEST_NOT(mpx.commit_write(8u));
   BOOST_TEST_EQ(
      mpx.get_write_buffer(),
      "PING\r\n$7\r\ncmd-arg\r\n"
      "*2\r\n$9\r\nSUBSCRIBE\r\n$7\r\ncmd-arg\r\n");
   BOOST_TEST(item1.elem_ptr->is_staged());
   BOOST_TEST(item2.elem_ptr->is_staged());

   // Write another part
   BOOST_TEST_NOT(mpx.commit_write(19u));
   BOOST_TEST_EQ(mpx.get_write_buffer(), "*2\r\n$9\r\nSUBSCRIBE\r\n$7\r\ncmd-arg\r\n");
   BOOST_TEST(item1.elem_ptr->is_staged());
   BOOST_TEST(item2.elem_ptr->is_staged());

   // A zero-size write doesn't cause trouble
   BOOST_TEST_NOT(mpx.commit_write(0u));
   BOOST_TEST_EQ(mpx.get_write_buffer(), "*2\r\n$9\r\nSUBSCRIBE\r\n$7\r\ncmd-arg\r\n");
   BOOST_TEST(item1.elem_ptr->is_staged());
   BOOST_TEST(item2.elem_ptr->is_staged());

   // Write everything except the last byte
   BOOST_TEST_NOT(mpx.commit_write(31u));
   BOOST_TEST_EQ(mpx.get_write_buffer(), "\n");
   BOOST_TEST(item1.elem_ptr->is_staged());
   BOOST_TEST(item2.elem_ptr->is_staged());

   // Write the last byte
   BOOST_TEST(mpx.commit_write(1u));
   BOOST_TEST(item1.elem_ptr->is_written());
   BOOST_TEST(item2.elem_ptr->is_done());
}

// The response to a request is received before its write
// confirmation. This might happen on heavy load
void test_request_response_before_write()
{
   // Setup
   auto item = std::make_unique<test_item>();
   multiplexer mpx;

   // Add the request and trigger the write
   mpx.add(item->elem_ptr);
   BOOST_TEST(item->elem_ptr->is_waiting());
   BOOST_TEST_EQ(mpx.prepare_write(), 1u);
   BOOST_TEST(item->elem_ptr->is_staged());
   BOOST_TEST(!item->done);

   // The response is received. The request is marked as done,
   // even if the write hasn't been confirmed yet
   error_code ec;
   read(mpx, "+one\r\n");
   auto ret = mpx.consume(ec);

   BOOST_TEST_EQ(ret.first, consume_result::got_response);
   BOOST_TEST_EQ(ec, error_code());
   BOOST_TEST(item->done);

   // The request is removed
   item.reset();

   // The write gets confirmed and causes no problem
   BOOST_TEST(mpx.commit_write(mpx.get_write_buffer().size()));
}

void test_push()
{
   // Setup
   multiplexer mpx;
   generic_response resp;
   mpx.set_receive_adapter(any_adapter{resp});

   // Consume an entire push
   error_code ec;
   read(mpx, ">2\r\n+one\r\n+two\r\n");
   auto const ret = mpx.consume(ec);

   // Check
   BOOST_TEST_EQ(ret.first, consume_result::got_push);
   BOOST_TEST_EQ(ret.second, 16u);
   const node expected[] = {
      {type::push,          2u, 0u, ""   },
      {type::simple_string, 1u, 1u, "one"},
      {type::simple_string, 1u, 1u, "two"},
   };
   check_response(resp, expected);
}

void test_push_needs_more()
{
   // Setup
   multiplexer mpx;
   generic_response resp;
   mpx.set_receive_adapter(any_adapter{resp});

   // Consume it only part of the message available.
   error_code ec;
   read(mpx, ">2\r\n+one\r");
   auto ret = mpx.consume(ec);

   BOOST_TEST_EQ(ret.first, consume_result::needs_more);

   // The entire message becomes available
   read(mpx, "\n+two\r\n");
   ret = mpx.consume(ec);

   BOOST_TEST_EQ(ret.first, consume_result::got_push);
   BOOST_TEST_EQ(ret.second, 16u);
   const node expected[] = {
      {type::push,          2u, 0u, ""   },
      {type::simple_string, 1u, 1u, "one"},
      {type::simple_string, 1u, 1u, "two"},
   };
   check_response(resp, expected);
}

// If a response is received and no request is waiting, it is interpreted
// as a push (e.g. MONITOR)
void test_push_heuristics_no_request()
{
   // Setup
   multiplexer mpx;
   generic_response resp;
   mpx.set_receive_adapter(any_adapter{resp});

   // Response received, but no request has been sent
   error_code ec;
   read(mpx, "+Hello world\r\n");
   auto const ret = mpx.consume(ec);

   // Check
   BOOST_TEST_EQ(ret.first, consume_result::got_push);
   BOOST_TEST_EQ(ret.second, 14u);
   const node expected[] = {
      {type::simple_string, 1u, 0u, "Hello world"},
   };
   check_response(resp, expected);
}

// Same, but there's another request that hasn't been written yet.
// This is an edge case but might happen due to race conditions.
void test_push_heuristics_request_waiting()
{
   // Setup
   multiplexer mpx;
   generic_response resp;
   mpx.set_receive_adapter(any_adapter{resp});
   test_item item;

   // Add the item but don't write it (e.g. the writer task is busy)
   mpx.add(item.elem_ptr);

   // Response received, but no request has been sent
   error_code ec;
   read(mpx, "+Hello world\r\n");
   auto const ret = mpx.consume(ec);

   // Check
   BOOST_TEST_EQ(ret.first, consume_result::got_push);
   BOOST_TEST_EQ(ret.second, 14u);
   const node expected[] = {
      {type::simple_string, 1u, 0u, "Hello world"},
   };
   check_response(resp, expected);
}

// If a response is received and the first request doesn't expect a response,
// it is interpreted as a push (e.g. SUBSCRIBE with incorrect syntax)
void test_push_heuristics_request_without_response()
{
   // Setup
   multiplexer mpx;
   generic_response resp;
   mpx.set_receive_adapter(any_adapter{resp});
   test_item item{false};

   // Add the request to the multiplexer
   mpx.add(item.elem_ptr);

   // Write it, but don't confirm the write, so the request doesn't get removed
   BOOST_TEST_EQ(mpx.prepare_write(), 1u);

   // Response received (e.g. syntax error)
   error_code ec;
   read(mpx, "-ERR wrong syntax\r\n");
   auto const ret = mpx.consume(ec);

   // Check
   BOOST_TEST_EQ(ret.first, consume_result::got_push);
   BOOST_TEST_EQ(ret.second, 19u);
   BOOST_TEST_EQ(resp.error().diagnostic, "ERR wrong syntax");
}

// We correctly reset parsing state between requests and pushes.
void test_mix_responses_pushes()
{
   // Setup
   multiplexer mpx;
   generic_response push_resp;
   mpx.set_receive_adapter(any_adapter{push_resp});
   test_item item1, item2;

   // Add the elements to the multiplexer and simulate a successful write
   mpx.add(item1.elem_ptr);
   mpx.add(item2.elem_ptr);
   BOOST_TEST_EQ(mpx.prepare_write(), 2u);
   BOOST_TEST(mpx.commit_write(mpx.get_write_buffer().size()));
   BOOST_TEST(item1.elem_ptr->is_written());
   BOOST_TEST(!item1.done);
   BOOST_TEST(item2.elem_ptr->is_written());
   BOOST_TEST(!item2.done);

   // Push
   std::string_view push1_buffer = ">2\r\n+one\r\n+two\r\n";
   error_code ec;
   read(mpx, push1_buffer);
   auto ret = mpx.consume(ec);
   BOOST_TEST_EQ(ret.first, consume_result::got_push);
   BOOST_TEST_EQ(ret.second, 16u);
   std::vector<node> expected{
      {type::push,          2u, 0u, ""   },
      {type::simple_string, 1u, 1u, "one"},
      {type::simple_string, 1u, 1u, "two"},
   };
   check_response(push_resp, expected);
   BOOST_TEST_NOT(item1.done);
   BOOST_TEST_NOT(item2.done);

   // First response
   std::string_view response1_buffer = "$11\r\nHello world\r\n";
   read(mpx, response1_buffer);
   ret = mpx.consume(ec);
   BOOST_TEST_EQ(ret.first, consume_result::got_response);
   BOOST_TEST_EQ(ret.second, 18u);
   expected = {
      {type::blob_string, 1u, 0u, "Hello world"},
   };
   check_response(item1.resp, expected);
   BOOST_TEST(item1.done);
   BOOST_TEST_NOT(item2.done);

   // Push
   std::string_view push2_buffer = ">2\r\n+other\r\n+push\r\n";
   read(mpx, push2_buffer);
   ret = mpx.consume(ec);
   BOOST_TEST_EQ(ret.first, consume_result::got_push);
   BOOST_TEST_EQ(ret.second, 19u);
   expected = {
      {type::push,          2u, 0u, ""     },
      {type::simple_string, 1u, 1u, "one"  },
      {type::simple_string, 1u, 1u, "two"  },
      {type::push,          2u, 0u, ""     },
      {type::simple_string, 1u, 1u, "other"},
      {type::simple_string, 1u, 1u, "push" },
   };
   check_response(push_resp, expected);
   BOOST_TEST(item1.done);
   BOOST_TEST_NOT(item2.done);

   // Second response
   std::string_view response2_buffer = "$8\r\nResponse\r\n";
   read(mpx, response2_buffer);
   ret = mpx.consume(ec);
   BOOST_TEST_EQ(ret.first, consume_result::got_response);
   BOOST_TEST_EQ(ret.second, 14u);
   expected = {
      {type::blob_string, 1u, 0u, "Response"},
   };
   check_response(item2.resp, expected);
   BOOST_TEST(item1.done);
   BOOST_TEST(item2.done);

   // Check usage
   const auto usg = mpx.get_usage();
   BOOST_TEST_EQ(usg.commands_sent, 2u);
   BOOST_TEST_EQ(usg.bytes_sent, item1.req.payload().size() + item2.req.payload().size());
   BOOST_TEST_EQ(usg.responses_received, 2u);
   BOOST_TEST_EQ(usg.pushes_received, 2u);
   BOOST_TEST_EQ(usg.response_bytes_received, response1_buffer.size() + response2_buffer.size());
   BOOST_TEST_EQ(usg.push_bytes_received, push1_buffer.size() + push2_buffer.size());
}

// Cancellation cases
// If the request is waiting, we just remove it
void test_cancel_waiting()
{
   // Setup
   multiplexer mpx;
   auto item1 = std::make_unique<test_item>();
   auto item2 = std::make_unique<test_item>();
   mpx.add(item1->elem_ptr);
   mpx.add(item2->elem_ptr);

   // Cancel the first request
   mpx.cancel(item1->elem_ptr);
   item1.reset();  // Verify we don't reference this item anyhow

   // We can progress the other request normally
   BOOST_TEST_EQ(mpx.prepare_write(), 1u);
   BOOST_TEST(mpx.commit_write(mpx.get_write_buffer().size()));
   error_code ec;
   read(mpx, "$11\r\nHello world\r\n");
   auto res = mpx.consume(ec);
   BOOST_TEST_EQ(res.first, consume_result::got_response);
   BOOST_TEST(item2->done);
   const node expected[] = {
      {type::blob_string, 1u, 0u, "Hello world"},
   };
   check_response(item2->resp, expected);
}

// If the request is staged, we mark it as abandoned
void test_cancel_staged()
{
   // Setup
   multiplexer mpx;
   auto item1 = std::make_unique<test_item>();
   auto item2 = std::make_unique<test_item>();
   mpx.add(item1->elem_ptr);
   mpx.add(item2->elem_ptr);

   // A write starts
   BOOST_TEST_EQ(mpx.prepare_write(), 2u);

   // Cancel the first request
   mpx.cancel(item1->elem_ptr);
   item1.reset();  // Verify we don't reference this item anyhow

   // The write gets confirmed
   BOOST_TEST(mpx.commit_write(mpx.get_write_buffer().size()));

   // The cancelled request's response arrives. It gets discarded
   error_code ec;
   read(mpx, "+Goodbye\r\n");
   auto res = mpx.consume(ec);
   BOOST_TEST_EQ(res.first, consume_result::got_response);
   BOOST_TEST_NOT(item2->done);

   // The 2nd request's response arrives. It gets parsed successfully
   read(mpx, "$11\r\nHello world\r\n");
   res = mpx.consume(ec);
   BOOST_TEST_EQ(res.first, consume_result::got_response);
   BOOST_TEST(item2->done);
   const node expected[] = {
      {type::blob_string, 1u, 0u, "Hello world"},
   };
   check_response(item2->resp, expected);
}

// If the request is staged but didn't expect a response, we remove it
void test_cancel_staged_command_without_response()
{
   // Setup
   multiplexer mpx;
   auto item1 = std::make_unique<test_item>(false);
   auto item2 = std::make_unique<test_item>();
   mpx.add(item1->elem_ptr);
   mpx.add(item2->elem_ptr);

   // A write starts
   BOOST_TEST_EQ(mpx.prepare_write(), 2u);

   // Cancel the first request
   mpx.cancel(item1->elem_ptr);
   item1.reset();  // Verify we don't reference this item anyhow

   // The write gets confirmed
   BOOST_TEST(mpx.commit_write(mpx.get_write_buffer().size()));

   // The 2nd request's response arrives. It gets parsed successfully
   error_code ec;
   read(mpx, "$11\r\nHello world\r\n");
   auto res = mpx.consume(ec);
   BOOST_TEST_EQ(res.first, consume_result::got_response);
   BOOST_TEST(item2->done);
   const node expected[] = {
      {type::blob_string, 1u, 0u, "Hello world"},
   };
   check_response(item2->resp, expected);
}

// If the request is written, we mark it as abandoned
void test_cancel_written()
{
   // Setup
   multiplexer mpx;
   auto item1 = std::make_unique<test_item>();
   auto item2 = std::make_unique<test_item>();
   mpx.add(item1->elem_ptr);
   mpx.add(item2->elem_ptr);

   // A write succeeds
   BOOST_TEST_EQ(mpx.prepare_write(), 2u);
   BOOST_TEST(mpx.commit_write(mpx.get_write_buffer().size()));

   // Cancel the first request
   mpx.cancel(item1->elem_ptr);
   item1.reset();  // Verify we don't reference this item anyhow

   // The cancelled request's response arrives. It gets discarded
   error_code ec;
   read(mpx, "+Goodbye\r\n");
   auto res = mpx.consume(ec);
   BOOST_TEST_EQ(res.first, consume_result::got_response);
   BOOST_TEST_NOT(item2->done);

   // The 2nd request's response arrives. It gets parsed successfully
   read(mpx, "$11\r\nHello world\r\n");
   res = mpx.consume(ec);
   BOOST_TEST_EQ(res.first, consume_result::got_response);
   BOOST_TEST(item2->done);
   const node expected[] = {
      {type::blob_string, 1u, 0u, "Hello world"},
   };
   check_response(item2->resp, expected);
}

// Having a written request for which part of its response
// has been received doesn't cause trouble
void test_cancel_written_half_parsed_response()
{
   // Setup
   request req;
   req.push("PING", "value1");
   req.push("PING", "value2");
   req.push("PING", "value3");
   multiplexer mpx;
   auto item1 = std::make_unique<test_item>(std::move(req));
   auto item2 = std::make_unique<test_item>();
   mpx.add(item1->elem_ptr);
   mpx.add(item2->elem_ptr);

   // A write succeeds
   BOOST_TEST_EQ(mpx.prepare_write(), 2u);
   BOOST_TEST(mpx.commit_write(mpx.get_write_buffer().size()));

   // Get the response for the 1st command in req1
   error_code ec;
   read(mpx, "+Goodbye\r\n");
   auto res = mpx.consume(ec);
   BOOST_TEST_EQ(res.first, consume_result::got_response);
   BOOST_TEST_NOT(item1->done);
   BOOST_TEST_EQ(ec, error_code());

   // Get a partial response for the 2nd command in req1
   read(mpx, "*2\r\n$4\r\nsome\r\n");
   res = mpx.consume(ec);
   BOOST_TEST_EQ(res.first, consume_result::needs_more);
   BOOST_TEST_NOT(item1->done);
   BOOST_TEST_EQ(ec, error_code());

   // Cancel the first request
   mpx.cancel(item1->elem_ptr);
   item1.reset();  // Verify we don't reference this item anyhow

   // Get the rest of the response for the 2nd command in req1
   read(mpx, "*2\r\n$4\r\nsome\r\n$4\r\ndata\r\n");
   res = mpx.consume(ec);
   BOOST_TEST_EQ(res.first, consume_result::got_response);
   BOOST_TEST_NOT(item2->done);
   BOOST_TEST_EQ(ec, error_code());

   // Get the response for the 3rd command in req1
   read(mpx, "+last\r\n");
   res = mpx.consume(ec);
   BOOST_TEST_EQ(res.first, consume_result::got_response);
   BOOST_TEST_NOT(item2->done);
   BOOST_TEST_EQ(ec, error_code());

   // Get the response for the 2nd request
   read(mpx, "$11\r\nHello world\r\n");
   res = mpx.consume(ec);
   BOOST_TEST_EQ(res.first, consume_result::got_response);
   BOOST_TEST(item2->done);
   const node expected[] = {
      {type::blob_string, 1u, 0u, "Hello world"},
   };
   check_response(item2->resp, expected);
}

// If an abandoned request receives a NULL or an error, nothing happens
// (regression check)
void test_cancel_written_null_error()
{
   // Setup
   request req;
   req.push("PING", "value1");
   req.push("PING", "value2");
   req.push("PING", "value3");
   multiplexer mpx;
   auto item1 = std::make_unique<test_item>(std::move(req));
   auto item2 = std::make_unique<test_item>();
   mpx.add(item1->elem_ptr);
   mpx.add(item2->elem_ptr);

   // A write succeeds
   BOOST_TEST_EQ(mpx.prepare_write(), 2u);
   BOOST_TEST(mpx.commit_write(mpx.get_write_buffer().size()));

   // Cancel the first request
   mpx.cancel(item1->elem_ptr);
   item1.reset();  // Verify we don't reference this item anyhow

   // The cancelled request's response arrives. It contains NULLs and errors.
   // We ignore them
   error_code ec;
   read(mpx, "-ERR wrong command\r\n");
   auto res = mpx.consume(ec);
   BOOST_TEST_EQ(res.first, consume_result::got_response);
   BOOST_TEST_EQ(ec, error_code());
   BOOST_TEST_NOT(item2->done);

   read(mpx, "!3\r\nBad\r\n");
   res = mpx.consume(ec);
   BOOST_TEST_EQ(res.first, consume_result::got_response);
   BOOST_TEST_EQ(ec, error_code());
   BOOST_TEST_NOT(item2->done);

   read(mpx, "_\r\n");
   res = mpx.consume(ec);
   BOOST_TEST_EQ(res.first, consume_result::got_response);
   BOOST_TEST_EQ(ec, error_code());
   BOOST_TEST_NOT(item2->done);

   // The 2nd request's response arrives. It gets parsed successfully
   read(mpx, "$11\r\nHello world\r\n");
   res = mpx.consume(ec);
   BOOST_TEST_EQ(res.first, consume_result::got_response);
   BOOST_TEST(item2->done);
   const node expected[] = {
      {type::blob_string, 1u, 0u, "Hello world"},
   };
   check_response(item2->resp, expected);
}

// Cancellation on connection lost
void test_cancel_on_connection_lost()
{
   // Setup
   multiplexer mpx;
   test_item item_written1, item_written2, item_staged1, item_staged2, item_waiting1, item_waiting2;

   // Different items have different configurations
   item_written1.req.get_config().cancel_if_unresponded = false;
   item_written1.req.get_config().cancel_on_connection_lost = true;
   item_written2.req.get_config().cancel_if_unresponded = true;
   item_written2.req.get_config().cancel_on_connection_lost = true;
   item_staged1.req.get_config().cancel_if_unresponded = false;
   item_staged1.req.get_config().cancel_on_connection_lost = true;
   item_staged2.req.get_config().cancel_if_unresponded = true;
   item_staged2.req.get_config().cancel_on_connection_lost = true;
   item_waiting1.req.get_config().cancel_if_unresponded = true;
   item_waiting1.req.get_config().cancel_on_connection_lost = false;
   item_waiting2.req.get_config().cancel_if_unresponded = true;
   item_waiting2.req.get_config().cancel_on_connection_lost = true;

   // Make each item reach the state it should be in
   mpx.add(item_written1.elem_ptr);
   mpx.add(item_written2.elem_ptr);
   BOOST_TEST_EQ(mpx.prepare_write(), 2u);
   BOOST_TEST(mpx.commit_write(mpx.get_write_buffer().size()));

   mpx.add(item_staged1.elem_ptr);
   mpx.add(item_staged2.elem_ptr);
   BOOST_TEST_EQ(mpx.prepare_write(), 2u);

   mpx.add(item_waiting1.elem_ptr);
   mpx.add(item_waiting2.elem_ptr);

   // Check that we got it right
   BOOST_TEST(item_written1.elem_ptr->is_written());
   BOOST_TEST(item_written2.elem_ptr->is_written());
   BOOST_TEST(item_staged1.elem_ptr->is_staged());
   BOOST_TEST(item_staged2.elem_ptr->is_staged());
   BOOST_TEST(item_waiting1.elem_ptr->is_waiting());
   BOOST_TEST(item_waiting2.elem_ptr->is_waiting());

   // Trigger a connection lost event
   mpx.cancel_on_conn_lost();

   // The ones with the cancellation settings set to false are back to waiting.
   // Others are cancelled
   BOOST_TEST(!item_written1.done);
   BOOST_TEST(item_written1.elem_ptr->is_waiting());
   BOOST_TEST(item_written2.done);
   BOOST_TEST(!item_staged1.done);
   BOOST_TEST(item_staged1.elem_ptr->is_waiting());
   BOOST_TEST(item_staged2.done);
   BOOST_TEST(!item_waiting1.done);
   BOOST_TEST(item_waiting1.elem_ptr->is_waiting());
   BOOST_TEST(item_waiting2.done);
}

// cancel_on_connection_lost cleans up any abandoned request,
// regardless of their configuration
void test_cancel_on_connection_lost_abandoned()
{
   // Setup
   multiplexer mpx;
   auto item_written1 = std::make_unique<test_item>();
   auto item_written2 = std::make_unique<test_item>();
   auto item_staged1 = std::make_unique<test_item>();
   auto item_staged2 = std::make_unique<test_item>();

   // Different items have different configurations
   item_written1->req.get_config().cancel_if_unresponded = false;
   item_written2->req.get_config().cancel_if_unresponded = true;
   item_staged1->req.get_config().cancel_if_unresponded = false;
   item_staged2->req.get_config().cancel_if_unresponded = true;

   // Make each item reach the state it should be in
   mpx.add(item_written1->elem_ptr);
   mpx.add(item_written2->elem_ptr);
   BOOST_TEST_EQ(mpx.prepare_write(), 2u);
   BOOST_TEST(mpx.commit_write(mpx.get_write_buffer().size()));

   mpx.add(item_staged1->elem_ptr);
   mpx.add(item_staged2->elem_ptr);
   BOOST_TEST_EQ(mpx.prepare_write(), 2u);

   // Check that we got it right
   BOOST_TEST(item_written1->elem_ptr->is_written());
   BOOST_TEST(item_written2->elem_ptr->is_written());
   BOOST_TEST(item_staged1->elem_ptr->is_staged());
   BOOST_TEST(item_staged2->elem_ptr->is_staged());

   // Cancel all of the requests
   mpx.cancel(item_written1->elem_ptr);
   mpx.cancel(item_written2->elem_ptr);
   mpx.cancel(item_staged1->elem_ptr);
   mpx.cancel(item_staged2->elem_ptr);
   item_written1.reset();
   item_written2.reset();
   item_staged1.reset();
   item_staged2.reset();

   // Trigger a connection lost event
   mpx.cancel_on_conn_lost();

   // This should have removed all requests, regardless of their config.
   // If we restore the connection and try a write, nothing gets written.
   mpx.reset();
   BOOST_TEST_EQ(mpx.prepare_write(), 0u);
}

// The test below fails. Uncomment when this is fixed:
// https://github.com/boostorg/redis/issues/307
// void test_cancel_on_connection_lost_half_parsed_response()
// {
//    // Setup
//    multiplexer mpx;
//    test_item item;
//    item.req.get_config().cancel_if_unresponded = false;
//    error_code ec;

//    // Add the request, write it and start parsing the response
//    mpx.add(item.elem_ptr);
//    BOOST_TEST_EQ(mpx.prepare_write(), 1u);
//    BOOST_TEST_EQ(mpx.commit_write(), 0u);
//    read(mpx, "*2\r\n+hello\r\n");
//    auto res = mpx.consume(ec);
//    BOOST_TEST_EQ(res.first, consume_result::needs_more);
//    BOOST_TEST_EQ(ec, error_code());

//    // Trigger a connection lost event
//    mpx.cancel_on_conn_lost();
//    BOOST_TEST(!item.done);
//    BOOST_TEST(item.elem_ptr->is_waiting());

//    // Simulate a reconnection
//    mpx.reset();

//    // Successful write, and this time the response is complete
//    BOOST_TEST_EQ(mpx.prepare_write(), 1u);
//    BOOST_TEST_EQ(mpx.commit_write(), 0u);
//    read(mpx, "*2\r\n+hello\r\n+world\r\n");
//    res = mpx.consume(ec);
//    BOOST_TEST_EQ(res.first, consume_result::got_response);
//    BOOST_TEST_EQ(ec, error_code());

//    // Check the response
//    const node expected[] = {
//       {type::array,         2u, 0u, ""     },
//       {type::simple_string, 1u, 1u, "hello"},
//       {type::simple_string, 1u, 1u, "world"},
//    };
//    check_response(item.resp, expected);
// }

// Resetting works
void test_reset()
{
   // Setup
   multiplexer mpx;
   generic_response push_resp;
   mpx.set_receive_adapter(any_adapter{push_resp});
   test_item item1, item2;
   item1.req.get_config().cancel_on_connection_lost = true;

   // Add a request
   mpx.add(item1.elem_ptr);

   // Start parsing a push
   error_code ec;
   read(mpx, ">2\r");
   auto ret = mpx.consume(ec);
   BOOST_TEST_EQ(ret.first, consume_result::needs_more);

   // Connection lost. The first request gets cancelled
   mpx.cancel_on_conn_lost();
   BOOST_TEST(item1.done);

   // Reconnection happens
   mpx.reset();
   ec.clear();

   // We're able to add write requests and read responses - all state was reset
   mpx.add(item2.elem_ptr);
   BOOST_TEST_EQ(mpx.prepare_write(), 1u);
   BOOST_TEST(mpx.commit_write(mpx.get_write_buffer().size()));

   std::string_view response_buffer = "$11\r\nHello world\r\n";
   read(mpx, response_buffer);
   ret = mpx.consume(ec);
   BOOST_TEST_EQ(ret.first, consume_result::got_response);
   BOOST_TEST_EQ(ret.second, response_buffer.size());
   const node expected[] = {
      {type::blob_string, 1u, 0u, "Hello world"},
   };
   check_response(item2.resp, expected);
   BOOST_TEST(item2.done);
}

}  // namespace

int main()
{
   test_request_needs_more();
   test_several_requests();
   test_request_response_before_write();
   test_short_writes();
   test_push();
   test_push_needs_more();
   test_push_heuristics_no_request();
   test_push_heuristics_request_without_response();
   test_push_heuristics_request_waiting();
   test_mix_responses_pushes();
   test_cancel_waiting();
   test_cancel_staged();
   test_cancel_staged_command_without_response();
   test_cancel_written();
   test_cancel_written_half_parsed_response();
   test_cancel_written_null_error();
   test_cancel_on_connection_lost();
   test_cancel_on_connection_lost_abandoned();
   // test_cancel_on_connection_lost_half_parsed_response();
   test_reset();

   return boost::report_errors();
}
