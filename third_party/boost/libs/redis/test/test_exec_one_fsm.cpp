//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/adapter/any_adapter.hpp>
#include <boost/redis/detail/exec_one_fsm.hpp>
#include <boost/redis/detail/read_buffer.hpp>
#include <boost/redis/error.hpp>
#include <boost/redis/resp3/node.hpp>
#include <boost/redis/resp3/type.hpp>

#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/error.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/system/error_code.hpp>

#include "print_node.hpp"

#include <iterator>
#include <ostream>
#include <string_view>
#include <vector>

using namespace boost::redis;
namespace asio = boost::asio;
using detail::exec_one_fsm;
using detail::exec_one_action;
using detail::exec_one_action_type;
using detail::read_buffer;
using boost::system::error_code;
using boost::asio::cancellation_type_t;
using parse_event = any_adapter::parse_event;
using resp3::type;

// Operators
static const char* to_string(exec_one_action_type value)
{
   switch (value) {
      case exec_one_action_type::done:      return "done";
      case exec_one_action_type::write:     return "write";
      case exec_one_action_type::read_some: return "read_some";
      default:                              return "<unknown writer_action_type>";
   }
}

namespace boost::redis::detail {

bool operator==(const exec_one_action& lhs, const exec_one_action& rhs) noexcept
{
   return lhs.type == rhs.type && lhs.ec == rhs.ec;
}

std::ostream& operator<<(std::ostream& os, const exec_one_action& act)
{
   os << "exec_one_action{ .type=" << to_string(act.type);
   if (act.type == exec_one_action_type::done)
      os << ", ec=" << act.ec;
   return os << " }";
}

}  // namespace boost::redis::detail

namespace {

struct adapter_event {
   parse_event type;
   resp3::node node{};

   friend bool operator==(const adapter_event& lhs, const adapter_event& rhs) noexcept
   {
      return lhs.type == rhs.type && lhs.node == rhs.node;
   }

   friend std::ostream& operator<<(std::ostream& os, const adapter_event& value)
   {
      switch (value.type) {
         case parse_event::init: return os << "adapter_event{ .type=init }";
         case parse_event::done: return os << "adapter_event{ .type=done }";
         case parse_event::node:
            return os << "adapter_event{ .type=node, .node=" << value.node << " }";
         default: return os << "adapter_event{ .type=unknown }";
      }
   }
};

any_adapter make_snoop_adapter(std::vector<adapter_event>& events)
{
   return any_adapter::impl_t{[&](parse_event ev, resp3::node_view const& nd, error_code&) {
      events.push_back({
         ev,
         {nd.data_type, nd.aggregate_size, nd.depth, std::string(nd.value)}
      });
   }};
}

void copy_to(read_buffer& buff, std::string_view data)
{
   auto const buffer = buff.get_prepared();
   BOOST_TEST_GE(buffer.size(), data.size());
   std::copy(data.cbegin(), data.cend(), buffer.begin());
}

void test_success()
{
   // Setup
   std::vector<adapter_event> events;
   exec_one_fsm fsm{make_snoop_adapter(events), 2u};
   read_buffer buff;

   // Write the request
   auto act = fsm.resume(buff, error_code(), 0u, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_one_action_type::write);

   // FSM should now ask for data
   act = fsm.resume(buff, error_code(), 25u, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_one_action_type::read_some);

   // Read the entire response in one go
   constexpr std::string_view payload = "$5\r\nhello\r\n*1\r\n+goodbye\r\n";
   copy_to(buff, payload);
   act = fsm.resume(buff, error_code(), payload.size(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_one_action_type::done);

   // Verify the adapter calls
   const adapter_event expected[] = {
      {parse_event::init},
      {parse_event::node, {type::blob_string, 1u, 0u, "hello"}},
      {parse_event::done},
      {parse_event::init},
      {parse_event::node, {type::array, 1u, 0u, ""}},
      {parse_event::node, {type::simple_string, 1u, 1u, "goodbye"}},
      {parse_event::done},
   };
   BOOST_TEST_ALL_EQ(events.begin(), events.end(), std::begin(expected), std::end(expected));
}

// The request didn't have any expected response (e.g. SUBSCRIBE)
void test_no_expected_response()
{
   // Setup
   std::vector<adapter_event> events;
   exec_one_fsm fsm{make_snoop_adapter(events), 0u};
   read_buffer buff;

   // Write the request
   auto act = fsm.resume(buff, error_code(), 0u, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_one_action_type::write);

   // FSM shouldn't ask for data
   act = fsm.resume(buff, error_code(), 25u, cancellation_type_t::none);
   BOOST_TEST_EQ(act, error_code());

   // No adapter calls should be done
   BOOST_TEST_EQ(events.size(), 0u);
}

// The response is scattered in several smaller fragments
void test_short_reads()
{
   // Setup
   std::vector<adapter_event> events;
   exec_one_fsm fsm{make_snoop_adapter(events), 2u};
   read_buffer buff;

   // Write the request
   auto act = fsm.resume(buff, error_code(), 0u, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_one_action_type::write);

   // FSM should now ask for data
   act = fsm.resume(buff, error_code(), 25u, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_one_action_type::read_some);

   // Read fragments
   constexpr std::string_view payload = "$5\r\nhello\r\n*1\r\n+goodbye\r\n";
   copy_to(buff, payload.substr(0, 6u));
   act = fsm.resume(buff, error_code(), 6u, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_one_action_type::read_some);

   copy_to(buff, payload.substr(6, 10u));
   act = fsm.resume(buff, error_code(), 10u, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_one_action_type::read_some);

   copy_to(buff, payload.substr(16));
   act = fsm.resume(buff, error_code(), payload.substr(16).size(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_one_action_type::done);

   // Verify the adapter calls
   const adapter_event expected[] = {
      {parse_event::init},
      {parse_event::node, {type::blob_string, 1u, 0u, "hello"}},
      {parse_event::done},
      {parse_event::init},
      {parse_event::node, {type::array, 1u, 0u, ""}},
      {parse_event::node, {type::simple_string, 1u, 1u, "goodbye"}},
      {parse_event::done},
   };
   BOOST_TEST_ALL_EQ(events.begin(), events.end(), std::begin(expected), std::end(expected));
}

// Errors in write
void test_write_error()
{
   // Setup
   std::vector<adapter_event> events;
   exec_one_fsm fsm{make_snoop_adapter(events), 2u};
   read_buffer buff;

   // Write the request
   auto act = fsm.resume(buff, error_code(), 0u, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_one_action_type::write);

   // Write error
   act = fsm.resume(buff, asio::error::connection_reset, 10u, cancellation_type_t::none);
   BOOST_TEST_EQ(act, error_code(asio::error::connection_reset));
}

void test_write_cancel()
{
   // Setup
   std::vector<adapter_event> events;
   exec_one_fsm fsm{make_snoop_adapter(events), 2u};
   read_buffer buff;

   // Write the request
   auto act = fsm.resume(buff, error_code(), 0u, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_one_action_type::write);

   // Edge case where the operation finished successfully but with the cancellation state set
   act = fsm.resume(buff, error_code(), 10u, cancellation_type_t::terminal);
   BOOST_TEST_EQ(act, error_code(asio::error::operation_aborted));
}

// Errors in read
void test_read_error()
{
   // Setup
   std::vector<adapter_event> events;
   exec_one_fsm fsm{make_snoop_adapter(events), 2u};
   read_buffer buff;

   // Write the request
   auto act = fsm.resume(buff, error_code(), 0u, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_one_action_type::write);

   // FSM should now ask for data
   act = fsm.resume(buff, error_code(), 25u, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_one_action_type::read_some);

   // Read error
   act = fsm.resume(buff, asio::error::network_reset, 0u, cancellation_type_t::none);
   BOOST_TEST_EQ(act, error_code(asio::error::network_reset));
}

void test_read_cancelled()
{
   // Setup
   std::vector<adapter_event> events;
   exec_one_fsm fsm{make_snoop_adapter(events), 2u};
   read_buffer buff;

   // Write the request
   auto act = fsm.resume(buff, error_code(), 0u, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_one_action_type::write);

   // FSM should now ask for data
   act = fsm.resume(buff, error_code(), 25u, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_one_action_type::read_some);

   // Edge case where the operation finished successfully but with the cancellation state set
   copy_to(buff, "$5\r\n");
   act = fsm.resume(buff, error_code(), 4u, cancellation_type_t::terminal);
   BOOST_TEST_EQ(act, error_code(asio::error::operation_aborted));
}

// Buffer too small
void test_buffer_prepare_error()
{
   // Setup
   std::vector<adapter_event> events;
   exec_one_fsm fsm{make_snoop_adapter(events), 2u};
   read_buffer buff;
   buff.set_config({4096u, 8u});  // max size is 8 bytes

   // Write the request
   auto act = fsm.resume(buff, error_code(), 0u, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_one_action_type::write);

   // When preparing the buffer, we encounter an error
   act = fsm.resume(buff, error_code(), 25u, cancellation_type_t::none);
   BOOST_TEST_EQ(act, error_code(error::exceeds_maximum_read_buffer_size));
}

// An invalid RESP3 message
void test_parse_error()
{
   // Setup
   std::vector<adapter_event> events;
   exec_one_fsm fsm{make_snoop_adapter(events), 2u};
   read_buffer buff;

   // Write the request
   auto act = fsm.resume(buff, error_code(), 0u, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_one_action_type::write);

   // FSM should now ask for data
   act = fsm.resume(buff, error_code(), 25u, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_one_action_type::read_some);

   // The response contains an invalid message
   constexpr std::string_view payload = "$bad\r\n";
   copy_to(buff, payload);
   act = fsm.resume(buff, error_code(), payload.size(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, error_code(error::not_a_number));
}

// Adapter signals an error
void test_adapter_error()
{
   // Setup. The adapter will fail in the 2nd node
   any_adapter adapter{[](parse_event ev, resp3::node_view const&, error_code& ec) {
      if (ev == parse_event::node)
         ec = error::empty_field;
   }};
   exec_one_fsm fsm{std::move(adapter), 2u};
   read_buffer buff;

   // Write the request
   auto act = fsm.resume(buff, error_code(), 0u, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_one_action_type::write);

   // FSM should now ask for data
   act = fsm.resume(buff, error_code(), 25u, cancellation_type_t::none);
   BOOST_TEST_EQ(act, exec_one_action_type::read_some);

   // Read the entire response in one go
   constexpr std::string_view payload = "$5\r\nhello\r\n*1\r\n+goodbye\r\n";
   copy_to(buff, payload);
   act = fsm.resume(buff, error_code(), payload.size(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, error_code(error::empty_field));
}

}  // namespace

int main()
{
   test_success();
   test_no_expected_response();
   test_short_reads();

   test_write_error();
   test_write_cancel();

   test_read_error();
   test_read_cancelled();

   test_buffer_prepare_error();
   test_parse_error();
   test_adapter_error();

   return boost::report_errors();
}
