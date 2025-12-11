//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/detail/exec_fsm.hpp>
#include <boost/redis/detail/multiplexer.hpp>
#include <boost/redis/request.hpp>

#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/error.hpp>
#include <boost/assert.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/system/error_code.hpp>

#include "sansio_utils.hpp"

#include <cstddef>
#include <memory>
#include <ostream>
#include <utility>

using namespace boost::redis;
namespace asio = boost::asio;
using detail::exec_fsm;
using detail::multiplexer;
using detail::exec_action_type;
using detail::consume_result;
using detail::exec_action;
using boost::system::error_code;
using boost::asio::cancellation_type_t;

#define BOOST_REDIS_EXEC_SWITCH_CASE(elem) \
   case exec_action_type::elem: return "exec_action_type::" #elem

static auto to_string(exec_action_type t) noexcept -> char const*
{
   switch (t) {
      BOOST_REDIS_EXEC_SWITCH_CASE(setup_cancellation);
      BOOST_REDIS_EXEC_SWITCH_CASE(immediate);
      BOOST_REDIS_EXEC_SWITCH_CASE(done);
      BOOST_REDIS_EXEC_SWITCH_CASE(notify_writer);
      BOOST_REDIS_EXEC_SWITCH_CASE(wait_for_response);
      default: return "exec_action_type::<invalid type>";
   }
}

// Operators
namespace boost::redis::detail {

std::ostream& operator<<(std::ostream& os, exec_action_type type)
{
   os << to_string(type);
   return os;
}

bool operator==(exec_action lhs, exec_action rhs) noexcept
{
   if (lhs.type() != rhs.type())
      return false;
   else if (lhs.type() == exec_action_type::done)
      return lhs.bytes_read() == rhs.bytes_read() && lhs.error() == rhs.error();
   else
      return true;
}

std::ostream& operator<<(std::ostream& os, exec_action act)
{
   os << "exec_action{ .type=" << act.type();
   if (act.type() == exec_action_type::done)
      os << ", .bytes_read=" << act.bytes_read() << ", .error=" << act.error();
   return os << " }";
}

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

// Prints a message on failure. Useful for parameterized tests
#define BOOST_TEST_EQ_MSG(lhs, rhs, msg)                                                     \
   if (!BOOST_TEST_EQ(lhs, rhs)) {                                                           \
      BOOST_LIGHTWEIGHT_TEST_OSTREAM << "Failure happened in context: " << msg << std::endl; \
   }

namespace {

// A helper to create a request and its associated elem
struct elem_and_request {
   request req;
   std::size_t done_calls{0u};  // number of times the done callback has been invoked
   std::shared_ptr<multiplexer::elem> elm;
   std::weak_ptr<multiplexer::elem> weak_elm;  // check that we free memory

   elem_and_request(request::config cfg = {})
   : req(cfg)
   {
      // Empty requests are not valid. The request needs to be populated before creating the element
      req.push("get", "mykey");
      elm = std::make_shared<multiplexer::elem>(req, any_adapter{});

      elm->set_done_callback([this] {
         ++done_calls;
      });

      weak_elm = elm;
   }
};

// The happy path
void test_success()
{
   // Setup
   multiplexer mpx;
   elem_and_request input;
   exec_fsm fsm(mpx, std::move(input.elm));
   error_code ec;

   // Initiate
   auto act = fsm.resume(true, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_action_type::setup_cancellation);
   act = fsm.resume(true, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_action_type::notify_writer);

   // We should now wait for a response
   act = fsm.resume(true, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_action_type::wait_for_response);

   // Simulate a successful write
   BOOST_TEST_EQ(mpx.prepare_write(), 1u);  // one request was placed in the packet to write
   BOOST_TEST(mpx.commit_write(mpx.get_write_buffer().size()));

   // Simulate a successful read
   read(mpx, "$5\r\nhello\r\n");
   auto req_status = mpx.consume(ec);
   BOOST_TEST_EQ(ec, error_code());
   BOOST_TEST_EQ(req_status.first, consume_result::got_response);
   BOOST_TEST_EQ(req_status.second, 11u);  // the entire buffer was consumed
   BOOST_TEST_EQ(input.done_calls, 1u);

   // This will awaken the exec operation, and should complete the operation
   act = fsm.resume(true, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_action(error_code(), 11u));

   // All memory should have been freed by now
   BOOST_TEST(input.weak_elm.expired());
}

// The request encountered an error while parsing
void test_parse_error()
{
   // Setup
   multiplexer mpx;
   elem_and_request input;
   exec_fsm fsm(mpx, std::move(input.elm));
   error_code ec;

   // Initiate
   auto act = fsm.resume(true, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_action_type::setup_cancellation);
   act = fsm.resume(true, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_action_type::notify_writer);

   // We should now wait for a response
   act = fsm.resume(true, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_action_type::wait_for_response);

   // Simulate a successful write
   BOOST_TEST_EQ(mpx.prepare_write(), 1u);  // one request was placed in the packet to write
   BOOST_TEST(mpx.commit_write(mpx.get_write_buffer().size()));

   // Simulate a read that will trigger an error.
   // The second field should be a number (rather than the empty string).
   // Note that although part of the buffer was consumed, the multiplexer
   // currently throws this information away.
   read(mpx, "*2\r\n$5\r\nhello\r\n:\r\n");
   auto req_status = mpx.consume(ec);
   BOOST_TEST_EQ(ec, error::empty_field);
   BOOST_TEST_EQ(req_status.second, 15u);
   BOOST_TEST_EQ(input.done_calls, 1u);

   // This will awaken the exec operation, and should complete the operation
   act = fsm.resume(true, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_action(error::empty_field, 0u));

   // All memory should have been freed by now
   BOOST_TEST(input.weak_elm.expired());
}

// The request was configured to be cancelled on connection error, and the connection is closed
void test_cancel_if_not_connected()
{
   // Setup
   multiplexer mpx;
   request::config cfg;
   cfg.cancel_if_not_connected = true;
   elem_and_request input(cfg);
   exec_fsm fsm(mpx, std::move(input.elm));

   // Initiate. We're not connected, so the request gets cancelled
   auto act = fsm.resume(false, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_action_type::immediate);

   act = fsm.resume(false, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_action(error::not_connected));

   // We didn't leave memory behind
   BOOST_TEST(input.weak_elm.expired());
}

// The connection is closed when we start the request, but the request was configured to wait
void test_not_connected()
{
   // Setup
   multiplexer mpx;
   elem_and_request input;
   exec_fsm fsm(mpx, std::move(input.elm));
   error_code ec;

   // Initiate
   auto act = fsm.resume(false, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_action_type::setup_cancellation);
   act = fsm.resume(true, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_action_type::notify_writer);

   // We should now wait for a response
   act = fsm.resume(true, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_action_type::wait_for_response);

   // Simulate a successful write
   BOOST_TEST_EQ(mpx.prepare_write(), 1u);  // one request was placed in the packet to write
   BOOST_TEST(mpx.commit_write(mpx.get_write_buffer().size()));

   // Simulate a successful read
   read(mpx, "$5\r\nhello\r\n");
   auto req_status = mpx.consume(ec);
   BOOST_TEST_EQ(ec, error_code());
   BOOST_TEST_EQ(req_status.first, consume_result::got_response);
   BOOST_TEST_EQ(req_status.second, 11u);  // the entire buffer was consumed
   BOOST_TEST_EQ(input.done_calls, 1u);

   // This will awaken the exec operation, and should complete the operation
   act = fsm.resume(true, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_action(error_code(), 11u));

   // All memory should have been freed by now
   BOOST_TEST(input.weak_elm.expired());
}

//
// Cancellations
//

// If the request is waiting, all cancellation types are supported
void test_cancel_waiting()
{
   constexpr struct {
      const char* name;
      asio::cancellation_type_t type;
   } test_cases[] = {
      {"terminal", asio::cancellation_type_t::terminal                                     },
      {"partial",  asio::cancellation_type_t::partial                                      },
      {"total",    asio::cancellation_type_t::total                                        },
      {"mixed",    asio::cancellation_type_t::partial | asio::cancellation_type_t::terminal},
      {"all",      asio::cancellation_type_t::all                                          },
   };

   for (const auto& tc : test_cases) {
      // Setup
      multiplexer mpx;
      elem_and_request input, input2;
      exec_fsm fsm(mpx, std::move(input.elm));

      // Another request enters the multiplexer, so it's busy when we start
      mpx.add(input2.elm);
      BOOST_TEST_EQ_MSG(mpx.prepare_write(), 1u, tc.name);

      // Initiate and wait
      auto act = fsm.resume(true, cancellation_type_t::none);
      BOOST_TEST_EQ_MSG(act, exec_action_type::setup_cancellation, tc.name);
      act = fsm.resume(true, cancellation_type_t::none);
      BOOST_TEST_EQ_MSG(act, exec_action_type::notify_writer, tc.name);
      act = fsm.resume(true, cancellation_type_t::none);
      BOOST_TEST_EQ_MSG(act, exec_action_type::wait_for_response, tc.name);

      // We get notified because the request got cancelled
      act = fsm.resume(true, tc.type);
      BOOST_TEST_EQ_MSG(act, exec_action(asio::error::operation_aborted), tc.name);
      BOOST_TEST_EQ_MSG(input.weak_elm.expired(), true, tc.name);  // we didn't leave memory behind
   }
}

// If the request is being processed and terminal or partial
// cancellation is requested, we mark the request as abandoned
void test_cancel_notwaiting_terminal_partial()
{
   constexpr struct {
      const char* name;
      asio::cancellation_type_t type;
   } test_cases[] = {
      {"terminal", asio::cancellation_type_t::terminal},
      {"partial",  asio::cancellation_type_t::partial },
   };

   for (const auto& tc : test_cases) {
      // Setup
      multiplexer mpx;
      auto input = std::make_unique<elem_and_request>();
      exec_fsm fsm(mpx, std::move(input->elm));

      // Initiate
      auto act = fsm.resume(false, cancellation_type_t::none);
      BOOST_TEST_EQ_MSG(act, exec_action_type::setup_cancellation, tc.name);
      act = fsm.resume(true, cancellation_type_t::none);
      BOOST_TEST_EQ_MSG(act, exec_action_type::notify_writer, tc.name);

      act = fsm.resume(true, cancellation_type_t::none);
      BOOST_TEST_EQ_MSG(act, exec_action_type::wait_for_response, tc.name);

      // The multiplexer starts writing the request
      BOOST_TEST_EQ_MSG(mpx.prepare_write(), 1u, tc.name);
      BOOST_TEST_EQ_MSG(mpx.commit_write(mpx.get_write_buffer().size()), true, tc.name);

      // A cancellation arrives
      act = fsm.resume(true, tc.type);
      BOOST_TEST_EQ(act, exec_action(asio::error::operation_aborted));
      input.reset();  // Verify we don't access the request or response after completion

      error_code ec;
      // When the response to this request arrives, it gets ignored
      read(mpx, "-ERR wrong command\r\n");
      auto res = mpx.consume(ec);
      BOOST_TEST_EQ_MSG(ec, error_code(), tc.name);
      BOOST_TEST_EQ_MSG(res.first, consume_result::got_response, tc.name);

      // The multiplexer::elem object needs to survive here to mark the
      // request as abandoned
   }
}

// If the request is being processed and total cancellation is requested, we ignore the cancellation
void test_cancel_notwaiting_total()
{
   // Setup
   multiplexer mpx;
   elem_and_request input;
   exec_fsm fsm(mpx, std::move(input.elm));
   error_code ec;

   // Initiate
   auto act = fsm.resume(true, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_action_type::setup_cancellation);
   act = fsm.resume(true, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_action_type::notify_writer);

   act = fsm.resume(true, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_action_type::wait_for_response);

   // Simulate a successful write
   BOOST_TEST_EQ(mpx.prepare_write(), 1u);
   BOOST_TEST(mpx.commit_write(mpx.get_write_buffer().size()));

   // We got requested a cancellation here, but we can't honor it
   act = fsm.resume(true, asio::cancellation_type_t::total);
   BOOST_TEST_EQ(act, exec_action_type::wait_for_response);

   // Simulate a successful read
   read(mpx, "$5\r\nhello\r\n");
   auto req_status = mpx.consume(ec);
   BOOST_TEST_EQ(ec, error_code());
   BOOST_TEST_EQ(req_status.first, consume_result::got_response);
   BOOST_TEST_EQ(req_status.second, 11u);  // the entire buffer was consumed
   BOOST_TEST_EQ(input.done_calls, 1u);

   // This will awaken the exec operation, and should complete the operation
   act = fsm.resume(true, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_action(error_code(), 11u));

   // All memory should have been freed by now
   BOOST_TEST_EQ(input.weak_elm.expired(), true);
}

}  // namespace

int main()
{
   test_success();
   test_parse_error();
   test_cancel_if_not_connected();
   test_not_connected();
   test_cancel_waiting();
   test_cancel_notwaiting_terminal_partial();
   test_cancel_notwaiting_total();

   return boost::report_errors();
}
