//
// Copyright (c) 2026 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/detail/connection_state.hpp>
#include <boost/redis/detail/receive_fsm.hpp>

#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/experimental/channel_error.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/system/error_code.hpp>

#include <iostream>
#include <string_view>

namespace net = boost::asio;
using namespace boost::redis;
using net::cancellation_type_t;
using boost::system::error_code;
using net::cancellation_type_t;
using detail::receive_action;
using detail::receive_fsm;
using detail::connection_state;
namespace channel_errc = net::experimental::channel_errc;
using action_type = receive_action::action_type;

// Operators
static const char* to_string(action_type type)
{
   switch (type) {
      case action_type::setup_cancellation: return "setup_cancellation";
      case action_type::wait:               return "wait";
      case action_type::drain_channel:      return "drain_channel";
      case action_type::immediate:          return "immediate";
      case action_type::done:               return "done";
      default:                              return "<unknown action::type>";
   }
}

namespace boost::redis::detail {

std::ostream& operator<<(std::ostream& os, action_type type) { return os << to_string(type); }

bool operator==(const receive_action& lhs, const receive_action& rhs) noexcept
{
   return lhs.type == rhs.type && lhs.ec == rhs.ec;
}

std::ostream& operator<<(std::ostream& os, const receive_action& act)
{
   os << "action{ .type=" << act.type;
   if (act.type == action_type::done)
      os << ", ec=" << act.ec;
   return os << " }";
}

}  // namespace boost::redis::detail

namespace {

struct fixture {
   connection_state st;
   generic_response resp;
};

void test_success()
{
   connection_state st;
   receive_fsm fsm;

   // Initiate
   auto act = fsm.resume(st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, action_type::setup_cancellation);
   act = fsm.resume(st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, action_type::wait);

   // At this point, the operation is now running
   BOOST_TEST(st.receive2_running);

   // The wait finishes successfully (we were notified). Receive exits
   act = fsm.resume(st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, action_type::drain_channel);
   act = fsm.resume(st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, error_code());

   // The operation is no longer running
   BOOST_TEST_NOT(st.receive2_running);
}

// We might see spurious cancels during reconnection (v1 compatibility).
void test_cancelled_reconnection()
{
   connection_state st;
   receive_fsm fsm;

   // Initiate
   auto act = fsm.resume(st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, action_type::setup_cancellation);
   act = fsm.resume(st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, action_type::wait);

   // Reconnection happens
   act = fsm.resume(st, channel_errc::channel_cancelled, cancellation_type_t::none);
   BOOST_TEST_EQ(act, action_type::wait);
   BOOST_TEST(st.receive2_running);  // still running

   // Another reconnection
   act = fsm.resume(st, channel_errc::channel_cancelled, cancellation_type_t::none);
   BOOST_TEST_EQ(act, action_type::wait);
   BOOST_TEST(st.receive2_running);  // still running

   // The wait finishes successfully (we were notified). Receive exits
   act = fsm.resume(st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, action_type::drain_channel);
   act = fsm.resume(st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, error_code());

   // The operation is no longer running
   BOOST_TEST_NOT(st.receive2_running);
}

// We might get cancellations due to connection::cancel()
void test_cancelled_connection_cancel()
{
   connection_state st;
   receive_fsm fsm;

   // Initiate
   auto act = fsm.resume(st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, action_type::setup_cancellation);
   act = fsm.resume(st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, action_type::wait);

   // Simulate a connection::cancel()
   st.receive2_cancelled = true;
   act = fsm.resume(st, channel_errc::channel_cancelled, cancellation_type_t::none);
   BOOST_TEST_EQ(act, error_code(net::error::operation_aborted));
   BOOST_TEST_NOT(st.receive2_running);
}

// Operations can still run after connection::cancel()
void test_after_connection_cancel()
{
   connection_state st;
   receive_fsm fsm;
   st.receive2_cancelled = true;

   // The operation initiates and runs normally
   auto act = fsm.resume(st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, action_type::setup_cancellation);
   act = fsm.resume(st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, action_type::wait);
   BOOST_TEST(st.receive2_running);

   // Reconnection behavior not affected
   act = fsm.resume(st, channel_errc::channel_cancelled, cancellation_type_t::none);
   BOOST_TEST_EQ(act, action_type::wait);
   BOOST_TEST(st.receive2_running);  // still running

   // Simulate a connection::cancel()
   st.receive2_cancelled = true;
   act = fsm.resume(st, channel_errc::channel_cancelled, cancellation_type_t::none);
   BOOST_TEST_EQ(act, error_code(net::error::operation_aborted));
   BOOST_TEST_NOT(st.receive2_running);
}

// Per-operation cancellation is supported
void test_per_operation_cancellation(std::string_view name, cancellation_type_t type)
{
   std::cerr << "Running cancellation case " << name << std::endl;

   connection_state st;
   receive_fsm fsm;

   // The operation initiates and runs normally
   auto act = fsm.resume(st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, action_type::setup_cancellation);
   act = fsm.resume(st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, action_type::wait);
   BOOST_TEST(st.receive2_running);

   // Cancellation is received
   act = fsm.resume(st, channel_errc::channel_cancelled, type);
   BOOST_TEST_EQ(act, error_code(net::error::operation_aborted));
   BOOST_TEST_NOT(st.receive2_running);
}

// Only a single instance of async_receive2 can be running at the same time
void test_error_already_running()
{
   connection_state st;
   receive_fsm fsm;
   st.receive2_running = true;

   // The operation fails immediately
   auto act = fsm.resume(st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, action_type::immediate);
   BOOST_TEST(st.receive2_running);  // not affected
   act = fsm.resume(st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, error_code(error::already_running));
   BOOST_TEST(st.receive2_running);  // not affected
}

// If an unknown error was obtained during channel receive, we propagate it
void test_error_unknown()
{
   connection_state st;
   receive_fsm fsm;

   // Initiate
   auto act = fsm.resume(st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, action_type::setup_cancellation);
   act = fsm.resume(st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, action_type::wait);
   BOOST_TEST(st.receive2_running);

   // We have an unknown error
   act = fsm.resume(st, channel_errc::channel_closed, cancellation_type_t::none);
   BOOST_TEST_EQ(act, error_code(channel_errc::channel_closed));
   BOOST_TEST_NOT(st.receive2_running);
}

}  // namespace

int main()
{
   test_success();
   test_cancelled_reconnection();
   test_cancelled_connection_cancel();
   test_after_connection_cancel();

   test_per_operation_cancellation("terminal", cancellation_type_t::terminal);
   test_per_operation_cancellation("partial", cancellation_type_t::partial);
   test_per_operation_cancellation("total", cancellation_type_t::total);
   test_per_operation_cancellation("all", cancellation_type_t::all);

   test_error_already_running();
   test_error_unknown();

   return boost::report_errors();
}
