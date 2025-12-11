//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/adapter/any_adapter.hpp>
#include <boost/redis/detail/connection_state.hpp>
#include <boost/redis/detail/reader_fsm.hpp>
#include <boost/redis/error.hpp>
#include <boost/redis/logger.hpp>

#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/error.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/system/error_code.hpp>

#include "sansio_utils.hpp"

#include <chrono>
#include <string_view>

namespace net = boost::asio;
namespace redis = boost::redis;
using boost::system::error_code;
using net::cancellation_type_t;
using redis::detail::reader_fsm;
using redis::detail::multiplexer;
using redis::generic_response;
using redis::any_adapter;
using redis::config;
using redis::detail::connection_state;
using action = redis::detail::reader_fsm::action;
using redis::logger;
using namespace std::chrono_literals;

// Operators
static const char* to_string(action::type type)
{
   switch (type) {
      case action::type::read_some:            return "action::type::read_some";
      case action::type::notify_push_receiver: return "action::type::notify_push_receiver";
      case action::type::done:                 return "action::type::done";
      default:                                 return "<unknown action::type>";
   }
}

namespace boost::redis::detail {

std::ostream& operator<<(std::ostream& os, action::type type) { return os << to_string(type); }

bool operator==(const action& lhs, const action& rhs) noexcept
{
   if (lhs.get_type() != rhs.get_type())
      return false;
   switch (lhs.get_type()) {
      case action::type::done:                 return lhs.error() == rhs.error();
      case action::type::read_some:            return lhs.timeout() == rhs.timeout();
      case action::type::notify_push_receiver: return lhs.push_size() == rhs.push_size();
      default:                                 BOOST_ASSERT(false); return false;
   }
}

std::ostream& operator<<(std::ostream& os, const action& act)
{
   auto t = act.get_type();
   os << "action{ .type=" << t;
   switch (t) {
      case action::type::done: os << ", .error=" << act.error(); break;
      case action::type::read_some:
         os << ", .timeout=" << to_milliseconds(act.timeout()) << "ms";
         break;
      case action::type::notify_push_receiver: os << ", .push_size=" << act.push_size(); break;
      default:                                 BOOST_ASSERT(false);
   }

   return os << " }";
}

}  // namespace boost::redis::detail

namespace {

// Copy data into the multiplexer with the following steps
//
//   1. get_read_buffer
//   2. Copy data in the buffer from 2.
//
// This is used in the reader_fsm tests.
void copy_to(multiplexer& mpx, std::string_view data)
{
   auto const buffer = mpx.get_prepared_read_buffer();
   BOOST_ASSERT(buffer.size() >= data.size());
   std::copy(data.cbegin(), data.cend(), buffer.begin());
}

struct fixture : redis::detail::log_fixture {
   connection_state st{{make_logger()}};
   generic_response resp;

   fixture()
   {
      st.mpx.set_receive_adapter(any_adapter{resp});
      st.cfg.health_check_interval = 3s;
   }
};

void test_push()
{
   fixture fix;
   reader_fsm fsm;

   // Initiate
   auto act = fsm.resume(fix.st, 0, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, action::read_some(6s));

   // The fsm is asking for data.
   std::string const payload =
      ">1\r\n+msg1\r\n"
      ">1\r\n+msg2 \r\n"
      ">1\r\n+msg3  \r\n";

   copy_to(fix.st.mpx, payload);

   // Deliver the 1st push
   act = fsm.resume(fix.st, payload.size(), error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, action::notify_push_receiver(11u));

   // Deliver the 2st push
   act = fsm.resume(fix.st, 0, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, action::notify_push_receiver(12u));

   // Deliver the 3rd push
   act = fsm.resume(fix.st, 0, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, action::notify_push_receiver(13u));

   // All pushes were delivered so the fsm should demand more data
   act = fsm.resume(fix.st, 0, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, action::read_some(6s));

   // Check logging
   fix.check_log({
      {logger::level::debug, "Reader task: issuing read" },
      {logger::level::debug, "Reader task: 36 bytes read"},
      {logger::level::debug, "Reader task: issuing read" },
   });
}

void test_read_needs_more()
{
   fixture fix;
   reader_fsm fsm;

   // Initiate
   auto act = fsm.resume(fix.st, 0, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, action::read_some(6s));

   // Split the incoming message in three random parts and deliver
   // them to the reader individually.
   std::string const msg[] = {">3\r", "\n+msg1\r\n+ms", "g2\r\n+msg3\r\n"};

   // Passes the first part to the fsm.
   copy_to(fix.st.mpx, msg[0]);
   act = fsm.resume(fix.st, msg[0].size(), error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, action::read_some(6s));

   // Passes the second part to the fsm.
   copy_to(fix.st.mpx, msg[1]);
   act = fsm.resume(fix.st, msg[1].size(), error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, action::read_some(6s));

   // Passes the third and last part to the fsm, next it should ask us
   // to deliver the message.
   copy_to(fix.st.mpx, msg[2]);
   act = fsm.resume(fix.st, msg[2].size(), error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, action::notify_push_receiver(msg[0].size() + msg[1].size() + msg[2].size()));

   // All pushes were delivered so the fsm should demand more data
   act = fsm.resume(fix.st, 0, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, action::read_some(6s));

   // Check logging
   fix.check_log({
      {logger::level::debug, "Reader task: issuing read"               },
      {logger::level::debug, "Reader task: 3 bytes read"               },
      {logger::level::debug, "Reader task: incomplete message received"},
      {logger::level::debug, "Reader task: issuing read"               },
      {logger::level::debug, "Reader task: 11 bytes read"              },
      {logger::level::debug, "Reader task: incomplete message received"},
      {logger::level::debug, "Reader task: issuing read"               },
      {logger::level::debug, "Reader task: 11 bytes read"              },
      {logger::level::debug, "Reader task: issuing read"               },
   });
}

void test_health_checks_disabled()
{
   fixture fix;
   reader_fsm fsm;
   fix.st.cfg.health_check_interval = 0s;

   // Initiate
   auto act = fsm.resume(fix.st, 0, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, action::read_some(0s));

   // Split the message into two so we cover both the regular read and the needs more branch
   constexpr std::string_view msg[] = {">3\r\n+msg1\r\n+ms", "g2\r\n+msg3\r\n"};

   // Passes the first part to the fsm.
   copy_to(fix.st.mpx, msg[0]);
   act = fsm.resume(fix.st, msg[0].size(), error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, action::read_some(0s));

   // Push delivery complete
   copy_to(fix.st.mpx, msg[1]);
   act = fsm.resume(fix.st, msg[1].size(), error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, action::notify_push_receiver(25u));

   // All pushes were delivered so the fsm should demand more data
   act = fsm.resume(fix.st, 0, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, action::read_some(0s));

   // Check logging
   fix.check_log({
      {logger::level::debug, "Reader task: issuing read"               },
      {logger::level::debug, "Reader task: 14 bytes read"              },
      {logger::level::debug, "Reader task: incomplete message received"},
      {logger::level::debug, "Reader task: issuing read"               },
      {logger::level::debug, "Reader task: 11 bytes read"              },
      {logger::level::debug, "Reader task: issuing read"               },
   });
}

void test_read_error()
{
   fixture fix;
   reader_fsm fsm;

   // Initiate
   auto act = fsm.resume(fix.st, 0, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, action::read_some(6s));

   // The fsm is asking for data.
   std::string const payload = ">1\r\n+msg1\r\n";
   copy_to(fix.st.mpx, payload);

   // Deliver the data
   act = fsm.resume(fix.st, payload.size(), {redis::error::empty_field}, cancellation_type_t::none);
   BOOST_TEST_EQ(act, error_code{redis::error::empty_field});

   // Check logging
   fix.check_log({
      // clang-format off
      {logger::level::debug, "Reader task: issuing read"        },
      {logger::level::debug, "Reader task: 11 bytes read, error: Expected field value is empty. [boost.redis:5]"},
      // clang-format on
   });
}

// A timeout in a read means that the connection is unhealthy (i.e. a PING timed out)
void test_read_timeout()
{
   fixture fix;
   reader_fsm fsm;

   // Initiate
   auto act = fsm.resume(fix.st, 0, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, action::read_some(6s));

   // Timeout
   act = fsm.resume(fix.st, 0, {net::error::operation_aborted}, cancellation_type_t::none);
   BOOST_TEST_EQ(act, error_code{redis::error::pong_timeout});

   // Check logging
   fix.check_log({
      // clang-format off
      {logger::level::debug, "Reader task: issuing read"        },
      {logger::level::debug, "Reader task: 0 bytes read, error: Pong timeout. [boost.redis:19]"},
      // clang-format on
   });
}

void test_parse_error()
{
   fixture fix;
   reader_fsm fsm;

   // Initiate
   auto act = fsm.resume(fix.st, 0, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, action::read_some(6s));

   // The fsm is asking for data.
   std::string const payload = ">a\r\n";
   copy_to(fix.st.mpx, payload);

   // Deliver the data
   act = fsm.resume(fix.st, payload.size(), {}, cancellation_type_t::none);
   BOOST_TEST_EQ(act, error_code{redis::error::not_a_number});

   // Check logging
   fix.check_log({
      {logger::level::debug, "Reader task: issuing read"},
      {logger::level::debug, "Reader task: 4 bytes read"},
      {logger::level::debug,
       "Reader task: error processing message: Can't convert string to number (maybe forgot to "
       "upgrade to RESP3?). [boost.redis:2]"            },
   });
}

void test_push_deliver_error()
{
   fixture fix;
   reader_fsm fsm;

   // Initiate
   auto act = fsm.resume(fix.st, 0, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, action::read_some(6s));

   // The fsm is asking for data.
   std::string const payload = ">1\r\n+msg1\r\n";
   copy_to(fix.st.mpx, payload);

   // Deliver the data
   act = fsm.resume(fix.st, payload.size(), {}, cancellation_type_t::none);
   BOOST_TEST_EQ(act, action::notify_push_receiver(11u));

   // Resumes from notifying a push with an error.
   act = fsm.resume(fix.st, 0, redis::error::empty_field, cancellation_type_t::none);
   BOOST_TEST_EQ(act, error_code{redis::error::empty_field});

   // Check logging
   fix.check_log({
      // clang-format off
      {logger::level::debug, "Reader task: issuing read"               },
      {logger::level::debug, "Reader task: 11 bytes read"               },
      {logger::level::debug, "Reader task: error notifying push receiver: Expected field value is empty. [boost.redis:5]"},
      // clang-format on
   });
}

void test_max_read_buffer_size()
{
   fixture fix;
   fix.st.cfg.read_buffer_append_size = 5;
   fix.st.cfg.max_read_size = 7;
   fix.st.mpx.set_config(fix.st.cfg);
   reader_fsm fsm;

   // Initiate
   auto act = fsm.resume(fix.st, 0, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, action::read_some(6s));

   // Passes the first part to the fsm.
   std::string const part1 = ">3\r\n";
   copy_to(fix.st.mpx, part1);
   act = fsm.resume(fix.st, part1.size(), error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, error_code(redis::error::exceeds_maximum_read_buffer_size));

   // Check logging
   fix.check_log({
      {logger::level::debug, "Reader task: issuing read"               },
      {logger::level::debug, "Reader task: 4 bytes read"               },
      {logger::level::debug, "Reader task: incomplete message received"},
      {logger::level::debug,
       "Reader task: error in prepare_read: Reading data from the socket would exceed the maximum "
       "size allowed of the read buffer. [boost.redis:26]"             },
   });
}

// Cancellations
void test_cancel_read()
{
   fixture fix;
   reader_fsm fsm;

   // Initiate
   auto act = fsm.resume(fix.st, 0, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, action::read_some(6s));

   // The read was cancelled (maybe after delivering some bytes)
   constexpr std::string_view payload = ">1\r\n";
   copy_to(fix.st.mpx, payload);
   act = fsm.resume(
      fix.st,
      payload.size(),
      net::error::operation_aborted,
      cancellation_type_t::terminal);
   BOOST_TEST_EQ(act, error_code(net::error::operation_aborted));

   // Check logging
   fix.check_log({
      {logger::level::debug, "Reader task: issuing read" },
      {logger::level::debug, "Reader task: cancelled (1)"},
   });
}

void test_cancel_read_edge()
{
   fixture fix;
   reader_fsm fsm;

   // Initiate
   auto act = fsm.resume(fix.st, 0, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, action::read_some(6s));

   // Deliver a push, and notify a cancellation.
   // This can happen if the cancellation signal arrives before the read handler runs
   constexpr std::string_view payload = ">1\r\n+msg1\r\n";
   copy_to(fix.st.mpx, payload);
   act = fsm.resume(fix.st, payload.size(), error_code(), cancellation_type_t::terminal);
   BOOST_TEST_EQ(act, error_code(net::error::operation_aborted));

   // Check logging
   fix.check_log({
      {logger::level::debug, "Reader task: issuing read" },
      {logger::level::debug, "Reader task: cancelled (1)"},
   });
}

void test_cancel_push_delivery()
{
   fixture fix;
   reader_fsm fsm;

   // Initiate
   auto act = fsm.resume(fix.st, 0, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, action::read_some(6s));

   // The fsm is asking for data.
   constexpr std::string_view payload =
      ">1\r\n+msg1\r\n"
      ">1\r\n+msg2 \r\n";

   copy_to(fix.st.mpx, payload);

   // Deliver the 1st push
   act = fsm.resume(fix.st, payload.size(), error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, action::notify_push_receiver(11u));

   // We got a cancellation while delivering it
   act = fsm.resume(fix.st, 0, net::error::operation_aborted, cancellation_type_t::terminal);
   BOOST_TEST_EQ(act, error_code(net::error::operation_aborted));

   // Check logging
   fix.check_log({
      {logger::level::debug, "Reader task: issuing read" },
      {logger::level::debug, "Reader task: 23 bytes read"},
      {logger::level::debug, "Reader task: cancelled (2)"},
   });
}

void test_cancel_push_delivery_edge()
{
   fixture fix;
   reader_fsm fsm;

   // Initiate
   auto act = fsm.resume(fix.st, 0, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, action::read_some(6s));

   // The fsm is asking for data.
   constexpr std::string_view payload =
      ">1\r\n+msg1\r\n"
      ">1\r\n+msg2 \r\n";

   copy_to(fix.st.mpx, payload);

   // Deliver the 1st push
   act = fsm.resume(fix.st, payload.size(), error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, action::notify_push_receiver(11u));

   // We got a cancellation after delivering it.
   // This can happen if the cancellation signal arrives before the channel send handler runs
   act = fsm.resume(fix.st, 0, error_code(), cancellation_type_t::terminal);
   BOOST_TEST_EQ(act, error_code(net::error::operation_aborted));

   // Check logging
   fix.check_log({
      {logger::level::debug, "Reader task: issuing read" },
      {logger::level::debug, "Reader task: 23 bytes read"},
      {logger::level::debug, "Reader task: cancelled (2)"},
   });
}

}  // namespace

int main()
{
   test_push();
   test_read_needs_more();
   test_health_checks_disabled();

   test_read_error();
   test_read_timeout();
   test_parse_error();
   test_push_deliver_error();
   test_max_read_buffer_size();

   test_cancel_read();
   test_cancel_read_edge();
   test_cancel_push_delivery();
   test_cancel_push_delivery_edge();

   return boost::report_errors();
}
