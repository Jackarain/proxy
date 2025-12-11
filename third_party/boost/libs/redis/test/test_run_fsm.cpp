//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/config.hpp>
#include <boost/redis/detail/connection_state.hpp>
#include <boost/redis/detail/multiplexer.hpp>
#include <boost/redis/detail/run_fsm.hpp>
#include <boost/redis/error.hpp>
#include <boost/redis/logger.hpp>

#include <boost/asio/error.hpp>
#include <boost/asio/local/basic_endpoint.hpp>  // for BOOST_ASIO_HAS_LOCAL_SOCKETS
#include <boost/core/lightweight_test.hpp>
#include <boost/system/error_code.hpp>

#include "sansio_utils.hpp"

#include <ostream>
#include <string_view>

using namespace boost::redis;
namespace asio = boost::asio;
using detail::run_fsm;
using detail::multiplexer;
using detail::run_action_type;
using detail::run_action;
using boost::system::error_code;
using boost::asio::cancellation_type_t;
using namespace std::chrono_literals;

// Operators
static const char* to_string(run_action_type value)
{
   switch (value) {
      case run_action_type::done:                  return "run_action_type::done";
      case run_action_type::immediate:             return "run_action_type::immediate";
      case run_action_type::connect:               return "run_action_type::connect";
      case run_action_type::parallel_group:        return "run_action_type::parallel_group";
      case run_action_type::cancel_receive:        return "run_action_type::cancel_receive";
      case run_action_type::wait_for_reconnection: return "run_action_type::wait_for_reconnection";
      default:                                     return "<unknown run_action_type>";
   }
}

namespace boost::redis::detail {

std::ostream& operator<<(std::ostream& os, run_action_type type)
{
   os << to_string(type);
   return os;
}

bool operator==(const run_action& lhs, const run_action& rhs) noexcept
{
   return lhs.type == rhs.type && lhs.ec == rhs.ec;
}

std::ostream& operator<<(std::ostream& os, const run_action& act)
{
   os << "run_action{ .type=" << act.type;
   if (act.type == run_action_type::done)
      os << ", .error=" << act.ec;
   return os << " }";
}

}  // namespace boost::redis::detail

namespace {

struct fixture : detail::log_fixture {
   detail::connection_state st;
   run_fsm fsm;

   static config default_config()
   {
      config res;
      res.use_setup = true;
      res.setup.clear();
      return res;
   }

   fixture(config&& cfg = default_config())
   : st{{make_logger()}, std::move(cfg)}
   { }
};

config config_no_reconnect()
{
   auto res = fixture::default_config();
   res.reconnect_wait_interval = 0s;
   return res;
}

// Config errors
#ifndef BOOST_ASIO_HAS_LOCAL_SOCKETS
void test_config_error_unix()
{
   // Setup
   config cfg;
   cfg.unix_socket = "/var/sock";
   fixture fix{std::move(cfg)};

   // Launching the operation fails immediately
   auto act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::immediate);
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, error_code(error::unix_sockets_unsupported));

   // Log
   fix.check_log({
      {logger::level::err,
       "Invalid configuration: The configuration specified a UNIX socket address, but UNIX sockets "
       "are not supported by the system. [boost.redis:24]"},
   });
}
#endif

void test_config_error_unix_ssl()
{
   // Setup
   config cfg;
   cfg.use_ssl = true;
   cfg.unix_socket = "/var/sock";
   fixture fix{std::move(cfg)};

   // Launching the operation fails immediately
   auto act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::immediate);
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, error_code(error::unix_sockets_ssl_unsupported));

   // Log
   fix.check_log({
      {logger::level::err,
       "Invalid configuration: The configuration specified UNIX sockets with SSL, which is not "
       "supported. [boost.redis:25]"},
   });
}

// An error in connect with reconnection enabled triggers a reconnection
void test_connect_error()
{
   // Setup
   fixture fix;

   // Launch the operation
   auto act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::connect);

   // Connect errors. We sleep and try to connect again
   act = fix.fsm.resume(fix.st, error::connect_timeout, cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::wait_for_reconnection);
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::connect);

   // This time we succeed and we launch the parallel group
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::parallel_group);

   // Run doesn't log, it's the subordinate tasks that do
   fix.check_log({});
}

// An error in connect without reconnection enabled makes the operation finish
void test_connect_error_no_reconnect()
{
   // Setup
   fixture fix{config_no_reconnect()};

   // Launch the operation
   auto act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::connect);

   // Connect errors. The operation finishes
   act = fix.fsm.resume(fix.st, error::connect_timeout, cancellation_type_t::none);
   BOOST_TEST_EQ(act, error_code(error::connect_timeout));

   // Run doesn't log, it's the subordinate tasks that do
   fix.check_log({});
}

// A cancellation in connect makes the operation finish even with reconnection enabled
void test_connect_cancel()
{
   // Setup
   fixture fix;

   // Launch the operation
   auto act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::connect);

   // Connect cancelled. The operation finishes
   act = fix.fsm.resume(fix.st, asio::error::operation_aborted, cancellation_type_t::terminal);
   BOOST_TEST_EQ(act, error_code(asio::error::operation_aborted));

   // We log on cancellation only
   fix.check_log({
      {logger::level::debug, "Run: cancelled (1)"}
   });
}

// Same, but only the cancellation is set
void test_connect_cancel_edge()
{
   // Setup
   fixture fix;

   // Launch the operation
   auto act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::connect);

   // Connect cancelled. The operation finishes
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::terminal);
   BOOST_TEST_EQ(act, error_code(asio::error::operation_aborted));

   // We log on cancellation only
   fix.check_log({
      {logger::level::debug, "Run: cancelled (1)"}
   });
}

// An error in the parallel group triggers a reconnection
// (the parallel group always exits with an error)
void test_parallel_group_error()
{
   // Setup
   fixture fix;

   // Run the operation. We connect and launch the tasks
   auto act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::connect);
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::parallel_group);

   // This exits with an error. We sleep and connect again
   act = fix.fsm.resume(fix.st, error::empty_field, cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::cancel_receive);
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::wait_for_reconnection);
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::connect);
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::parallel_group);

   // Run doesn't log, it's the subordinate tasks that do
   fix.check_log({});
}

// An error in the parallel group makes the operation exit if reconnection is disabled
void test_parallel_group_error_no_reconnect()
{
   // Setup
   fixture fix{config_no_reconnect()};

   // Run the operation. We connect and launch the tasks
   auto act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::connect);
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::parallel_group);

   // This exits with an error. We cancel the receive operation and exit
   act = fix.fsm.resume(fix.st, error::empty_field, cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::cancel_receive);
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, error_code(error::empty_field));

   // Run doesn't log, it's the subordinate tasks that do
   fix.check_log({});
}

// A cancellation in the parallel group makes it exit, even if reconnection is enabled.
// Parallel group tasks always exit with an error, so there is no edge case here
void test_parallel_group_cancel()
{
   // Setup
   fixture fix;

   // Run the operation. We connect and launch the tasks
   auto act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::connect);
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::parallel_group);

   // This exits because the operation gets cancelled. Any receive operation gets cancelled
   act = fix.fsm.resume(fix.st, asio::error::operation_aborted, cancellation_type_t::terminal);
   BOOST_TEST_EQ(act, run_action_type::cancel_receive);
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::terminal);
   BOOST_TEST_EQ(act, error_code(asio::error::operation_aborted));

   // We log on cancellation only
   fix.check_log({
      {logger::level::debug, "Run: cancelled (2)"}
   });
}

void test_parallel_group_cancel_no_reconnect()
{
   // Setup
   fixture fix{config_no_reconnect()};

   // Run the operation. We connect and launch the tasks
   auto act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::connect);
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::parallel_group);

   // This exits because the operation gets cancelled. Any receive operation gets cancelled
   act = fix.fsm.resume(fix.st, asio::error::operation_aborted, cancellation_type_t::terminal);
   BOOST_TEST_EQ(act, run_action_type::cancel_receive);
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::terminal);
   BOOST_TEST_EQ(act, error_code(asio::error::operation_aborted));

   // We log on cancellation only
   fix.check_log({
      {logger::level::debug, "Run: cancelled (2)"}
   });
}

// If the reconnection wait gets cancelled, we exit
void test_wait_cancel()
{
   // Setup
   fixture fix;

   // Run the operation. We connect and launch the tasks
   auto act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::connect);
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::parallel_group);

   // This exits with an error. We sleep
   act = fix.fsm.resume(fix.st, error::empty_field, cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::cancel_receive);
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::wait_for_reconnection);

   // We get cancelled during the sleep
   act = fix.fsm.resume(fix.st, asio::error::operation_aborted, cancellation_type_t::terminal);
   BOOST_TEST_EQ(act, error_code(asio::error::operation_aborted));

   // We log on cancellation only
   fix.check_log({
      {logger::level::debug, "Run: cancelled (3)"}
   });
}

void test_wait_cancel_edge()
{
   // Setup
   fixture fix;

   // Run the operation. We connect and launch the tasks
   auto act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::connect);
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::parallel_group);

   // This exits with an error. We sleep
   act = fix.fsm.resume(fix.st, error::empty_field, cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::cancel_receive);
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::wait_for_reconnection);

   // We get cancelled during the sleep
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::terminal);
   BOOST_TEST_EQ(act, error_code(asio::error::operation_aborted));

   // We log on cancellation only
   fix.check_log({
      {logger::level::debug, "Run: cancelled (3)"}
   });
}

void test_several_reconnections()
{
   // Setup
   fixture fix;

   // Run the operation. Connect errors and we sleep
   auto act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::connect);
   act = fix.fsm.resume(fix.st, error::connect_timeout, cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::wait_for_reconnection);

   // Connect again, this time successfully. We launch the tasks
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::connect);
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::parallel_group);

   // This exits with an error. We sleep and connect again
   act = fix.fsm.resume(fix.st, error::empty_field, cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::cancel_receive);
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::wait_for_reconnection);
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::connect);
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::parallel_group);

   // Exit with cancellation
   act = fix.fsm.resume(fix.st, asio::error::operation_aborted, cancellation_type_t::terminal);
   BOOST_TEST_EQ(act, run_action_type::cancel_receive);
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::terminal);
   BOOST_TEST_EQ(act, error_code(asio::error::operation_aborted));

   // The cancellation was logged
   fix.check_log({
      {logger::level::debug, "Run: cancelled (2)"}
   });
}

// Setup and ping requests are only composed once at startup
void test_setup_ping_requests()
{
   // Setup
   config cfg;
   cfg.health_check_id = "some_value";
   cfg.username = "foo";
   cfg.password = "bar";
   cfg.clientname = "";
   fixture fix{std::move(cfg)};

   // Run the operation. We connect and launch the tasks
   auto act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::connect);
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::parallel_group);

   // At this point, the requests are set up
   const std::string_view expected_ping = "*2\r\n$4\r\nPING\r\n$10\r\nsome_value\r\n";
   const std::string_view
      expected_setup = "*5\r\n$5\r\nHELLO\r\n$1\r\n3\r\n$4\r\nAUTH\r\n$3\r\nfoo\r\n$3\r\nbar\r\n";
   BOOST_TEST_EQ(fix.st.ping_req.payload(), expected_ping);
   BOOST_TEST_EQ(fix.st.cfg.setup.payload(), expected_setup);

   // Reconnect
   act = fix.fsm.resume(fix.st, error::empty_field, cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::cancel_receive);
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::wait_for_reconnection);
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::connect);
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::parallel_group);

   // The requests haven't been modified
   BOOST_TEST_EQ(fix.st.ping_req.payload(), expected_ping);
   BOOST_TEST_EQ(fix.st.cfg.setup.payload(), expected_setup);
}

// We correctly send and log the setup request
void test_setup_request_success()
{
   // Setup
   fixture fix;
   fix.st.cfg.setup.clear();
   fix.st.cfg.setup.push("HELLO", 3);

   // Run the operation. We connect and launch the tasks
   auto act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::connect);
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::parallel_group);

   // At this point, the setup request should be already queued. Simulate the writer
   BOOST_TEST_EQ(fix.st.mpx.prepare_write(), 1u);
   BOOST_TEST(fix.st.mpx.commit_write(fix.st.mpx.get_write_buffer().size()));

   // Simulate a successful read
   read(fix.st.mpx, "+OK\r\n");
   error_code ec;
   auto res = fix.st.mpx.consume(ec);
   BOOST_TEST_EQ(ec, error_code());
   BOOST_TEST(res.first == detail::consume_result::got_response);

   // Check log
   fix.check_log({
      {logger::level::info, "Setup request execution: success"}
   });
}

// We don't send empty setup requests
void test_setup_request_empty()
{
   // Setup
   fixture fix;
   fix.st.cfg.setup.clear();

   // Run the operation. We connect and launch the tasks
   auto act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::connect);
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::parallel_group);

   // Nothing was added to the multiplexer
   BOOST_TEST_EQ(fix.st.mpx.prepare_write(), 0u);

   // Check log
   fix.check_log({});
}

// A server error would cause the reader to exit
void test_setup_request_server_error()
{
   // Setup
   fixture fix;
   fix.st.setup_diagnostic = "leftover";  // simulate a leftover from previous runs
   fix.st.cfg.setup.clear();
   fix.st.cfg.setup.push("HELLO", 3);

   // Run the operation. We connect and launch the tasks
   auto act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::connect);
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::parallel_group);

   // At this point, the setup request should be already queued. Simulate the writer
   BOOST_TEST_EQ(fix.st.mpx.prepare_write(), 1u);
   BOOST_TEST(fix.st.mpx.commit_write(fix.st.mpx.get_write_buffer().size()));

   // Simulate a successful read
   read(fix.st.mpx, "-ERR: wrong command\r\n");
   error_code ec;
   auto res = fix.st.mpx.consume(ec);
   BOOST_TEST_EQ(ec, error::resp3_hello);
   BOOST_TEST(res.first == detail::consume_result::got_response);

   // Check log
   fix.check_log({
      {logger::level::info,
       "Setup request execution: The server response to the setup request sent during connection "
       "establishment contains an error. [boost.redis:23] (ERR: wrong command)"}
   });
}

}  // namespace

int main()
{
#ifndef BOOST_ASIO_HAS_LOCAL_SOCKETS
   test_config_error_unix();
#endif
   test_config_error_unix_ssl();

   test_connect_error();
   test_connect_error_no_reconnect();
   test_connect_cancel();
   test_connect_cancel_edge();

   test_parallel_group_error();
   test_parallel_group_error_no_reconnect();
   test_parallel_group_cancel();
   test_parallel_group_cancel_no_reconnect();

   test_wait_cancel();
   test_wait_cancel_edge();

   test_several_reconnections();
   test_setup_ping_requests();

   test_setup_request_success();
   test_setup_request_empty();
   test_setup_request_server_error();

   return boost::report_errors();
}
