//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/config.hpp>
#include <boost/redis/detail/connect_fsm.hpp>
#include <boost/redis/error.hpp>
#include <boost/redis/logger.hpp>

#include <boost/asio/error.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/core/lightweight_test.hpp>

#include "sansio_utils.hpp"

#include <iterator>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

using namespace boost::redis;
namespace asio = boost::asio;
using detail::connect_fsm;
using detail::connect_action_type;
using detail::connect_action;
using detail::buffered_logger;
using detail::redis_stream_state;
using detail::transport_type;
using asio::ip::tcp;
using boost::system::error_code;
using boost::asio::cancellation_type_t;
using resolver_results = tcp::resolver::results_type;

// Operators
static const char* to_string(connect_action_type type)
{
   switch (type) {
      case connect_action_type::unix_socket_close: return "connect_action_type::unix_socket_close";
      case connect_action_type::unix_socket_connect:
         return "connect_action_type::unix_socket_connect";
      case connect_action_type::tcp_resolve:      return "connect_action_type::tcp_resolve";
      case connect_action_type::tcp_connect:      return "connect_action_type::tcp_connect";
      case connect_action_type::ssl_stream_reset: return "connect_action_type::ssl_stream_reset";
      case connect_action_type::ssl_handshake:    return "connect_action_type::ssl_handshake";
      case connect_action_type::done:             return "connect_action_type::done";
      default:                                    return "<unknown connect_action_type>";
   }
}

static const char* to_string(transport_type type)
{
   switch (type) {
      case transport_type::tcp:         return "transport_type::tcp";
      case transport_type::tcp_tls:     return "transport_type::tcp_tls";
      case transport_type::unix_socket: return "transport_type::unix_socket";
      default:                          return "<unknown transport_type>";
   }
}

namespace boost::redis::detail {

std::ostream& operator<<(std::ostream& os, connect_action_type type)
{
   return os << to_string(type);
}

std::ostream& operator<<(std::ostream& os, transport_type type) { return os << to_string(type); }

bool operator==(const connect_action& lhs, const connect_action& rhs) noexcept
{
   return lhs.type == rhs.type && lhs.ec == rhs.ec;
}

std::ostream& operator<<(std::ostream& os, const connect_action& act)
{
   os << "connect_action{ .type=" << act.type;
   if (act.type == connect_action_type::done)
      os << ", .error=" << act.ec;
   return os << " }";
}

}  // namespace boost::redis::detail

namespace {

// TCP endpoints
const tcp::endpoint endpoint(asio::ip::make_address("192.168.10.1"), 1234);
const tcp::endpoint endpoint2(asio::ip::make_address("192.168.10.2"), 1235);

auto resolver_data = [] {
   const tcp::endpoint data[] = {endpoint, endpoint2};
   return asio::ip::tcp::resolver::results_type::create(
      std::begin(data),
      std::end(data),
      "my_host",
      "1234");
}();

// Reduce duplication
struct fixture : detail::log_fixture {
   config cfg;
   buffered_logger lgr{make_logger()};
   connect_fsm fsm{cfg, lgr};
   redis_stream_state st{};

   fixture(config&& cfg = {})
   : cfg{std::move(cfg)}
   { }
};

config make_ssl_config()
{
   config cfg;
   cfg.use_ssl = true;
   return cfg;
}

config make_unix_config()
{
   config cfg;
   cfg.unix_socket = "/run/redis.sock";
   return cfg;
}

void test_tcp_success()
{
   // Setup
   fixture fix;

   // Run the algorithm
   auto act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_resolve);
   act = fix.fsm.resume(error_code(), resolver_data, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_connect);
   act = fix.fsm.resume(error_code(), endpoint, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::done);

   // The transport type was appropriately set
   BOOST_TEST_EQ(fix.st.type, transport_type::tcp);
   BOOST_TEST_NOT(fix.st.ssl_stream_used);

   // Check logging
   fix.check_log({
      {logger::level::info, "Resolve results: 192.168.10.1:1234, 192.168.10.2:1235"},
      {logger::level::info, "Connected to 192.168.10.1:1234"                       },
   });
}

void test_tcp_tls_success()
{
   // Setup
   fixture fix{make_ssl_config()};

   // Run the algorithm. No SSL stream reset is performed here
   auto act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_resolve);
   act = fix.fsm.resume(error_code(), resolver_data, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_connect);
   act = fix.fsm.resume(error_code(), endpoint, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::ssl_handshake);
   act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::done);

   // The transport type was appropriately set
   BOOST_TEST_EQ(fix.st.type, transport_type::tcp_tls);
   BOOST_TEST(fix.st.ssl_stream_used);

   // Check logging
   fix.check_log({
      {logger::level::info, "Resolve results: 192.168.10.1:1234, 192.168.10.2:1235"},
      {logger::level::info, "Connected to 192.168.10.1:1234"                       },
      {logger::level::info, "Successfully performed SSL handshake"                 },
   });
}

void test_tcp_tls_success_reconnect()
{
   // Setup
   fixture fix{make_ssl_config()};
   fix.st.ssl_stream_used = true;

   // Run the algorithm. The stream is used, so it needs to be reset
   auto act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::ssl_stream_reset);
   act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_resolve);
   act = fix.fsm.resume(error_code(), resolver_data, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_connect);
   act = fix.fsm.resume(error_code(), endpoint, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::ssl_handshake);
   act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::done);

   // The transport type was appropriately set
   BOOST_TEST_EQ(fix.st.type, transport_type::tcp_tls);
   BOOST_TEST(fix.st.ssl_stream_used);

   // Check logging
   fix.check_log({
      {logger::level::info, "Resolve results: 192.168.10.1:1234, 192.168.10.2:1235"},
      {logger::level::info, "Connected to 192.168.10.1:1234"                       },
      {logger::level::info, "Successfully performed SSL handshake"                 },
   });
}

void test_unix_success()
{
   // Setup
   fixture fix{make_unix_config()};

   // Run the algorithm
   auto act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::unix_socket_close);
   act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::unix_socket_connect);
   act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::done);

   // The transport type was appropriately set
   BOOST_TEST_EQ(fix.st.type, transport_type::unix_socket);
   BOOST_TEST_NOT(fix.st.ssl_stream_used);

   // Check logging
   fix.check_log({
      {logger::level::info, "Connected to /run/redis.sock"},
   });
}

// Close errors are ignored
void test_unix_success_close_error()
{
   // Setup
   fixture fix{make_unix_config()};

   // Run the algorithm
   auto act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::unix_socket_close);
   act = fix.fsm.resume(asio::error::bad_descriptor, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::unix_socket_connect);
   act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::done);

   // The transport type was appropriately set
   BOOST_TEST_EQ(fix.st.type, transport_type::unix_socket);
   BOOST_TEST_NOT(fix.st.ssl_stream_used);

   // Check logging
   fix.check_log({
      {logger::level::info, "Connected to /run/redis.sock"},
   });
}

// Resolve errors
void test_tcp_resolve_error()
{
   // Setup
   fixture fix;

   // Run the algorithm
   auto act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_resolve);
   act = fix.fsm.resume(error::empty_field, resolver_results{}, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, error_code(error::empty_field));

   // Check logging
   fix.check_log({
      // clang-format off
      {logger::level::info, "Error resolving the server hostname: Expected field value is empty. [boost.redis:5]"},
      // clang-format on
   });
}

void test_tcp_resolve_timeout()
{
   // Setup
   fixture fix;

   // Since we use cancel_after, a timeout is an operation_aborted without a cancellation state set
   auto act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_resolve);
   act = fix.fsm.resume(
      asio::error::operation_aborted,
      resolver_results{},
      fix.st,
      cancellation_type_t::none);
   BOOST_TEST_EQ(act, error_code(error::resolve_timeout));

   // Check logging
   fix.check_log({
      // clang-format off
      {logger::level::info, "Error resolving the server hostname: Resolve timeout. [boost.redis:17]"},
      // clang-format on
   });
}

void test_tcp_resolve_cancel()
{
   // Setup
   fixture fix;

   // Run the algorithm
   auto act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_resolve);
   act = fix.fsm.resume(
      asio::error::operation_aborted,
      resolver_results{},
      fix.st,
      cancellation_type_t::terminal);
   BOOST_TEST_EQ(act, error_code(asio::error::operation_aborted));

   // Logging here is system-dependent, so we don't check the message
   BOOST_TEST_EQ(fix.msgs.size(), 1u);
}

void test_tcp_resolve_cancel_edge()
{
   // Setup
   fixture fix;

   // Cancel state set but no error
   auto act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_resolve);
   act = fix.fsm.resume(error_code(), resolver_results{}, fix.st, cancellation_type_t::terminal);
   BOOST_TEST_EQ(act, error_code(asio::error::operation_aborted));

   // Logging here is system-dependent, so we don't check the message
   BOOST_TEST_EQ(fix.msgs.size(), 1u);
}

// Connect errors
void test_tcp_connect_error()
{
   // Setup
   fixture fix;

   // Run the algorithm
   auto act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_resolve);
   act = fix.fsm.resume(error_code(), resolver_data, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_connect);
   act = fix.fsm.resume(error::empty_field, tcp::endpoint{}, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, error_code(error::empty_field));

   // Check logging
   fix.check_log({
      // clang-format off
      {logger::level::info, "Resolve results: 192.168.10.1:1234, 192.168.10.2:1235"},
      {logger::level::info, "Failed to connect to the server: Expected field value is empty. [boost.redis:5]"},
      // clang-format on
   });
}

void test_tcp_connect_timeout()
{
   // Setup
   fixture fix;

   // Run the algorithm
   auto act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_resolve);
   act = fix.fsm.resume(error_code(), resolver_data, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_connect);
   act = fix.fsm.resume(
      asio::error::operation_aborted,
      tcp::endpoint{},
      fix.st,
      cancellation_type_t::none);
   BOOST_TEST_EQ(act, error_code(error::connect_timeout));

   // Check logging
   fix.check_log({
      // clang-format off
      {logger::level::info, "Resolve results: 192.168.10.1:1234, 192.168.10.2:1235"},
      {logger::level::info, "Failed to connect to the server: Connect timeout. [boost.redis:18]"},
      // clang-format on
   });
}

void test_tcp_connect_cancel()
{
   // Setup
   fixture fix;

   // Run the algorithm
   auto act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_resolve);
   act = fix.fsm.resume(error_code(), resolver_data, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_connect);
   act = fix.fsm.resume(
      asio::error::operation_aborted,
      tcp::endpoint{},
      fix.st,
      cancellation_type_t::terminal);
   BOOST_TEST_EQ(act, error_code(asio::error::operation_aborted));

   // Logging here is system-dependent, so we don't check the message
   BOOST_TEST_EQ(fix.msgs.size(), 2u);
}

void test_tcp_connect_cancel_edge()
{
   // Setup
   fixture fix;

   // Run the algorithm. Cancellation state set but no error
   auto act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_resolve);
   act = fix.fsm.resume(error_code(), resolver_data, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_connect);
   act = fix.fsm.resume(error_code(), tcp::endpoint{}, fix.st, cancellation_type_t::terminal);
   BOOST_TEST_EQ(act, error_code(asio::error::operation_aborted));

   // Logging here is system-dependent, so we don't check the message
   BOOST_TEST_EQ(fix.msgs.size(), 2u);
}

// SSL handshake error
void test_ssl_handshake_error()
{
   // Setup
   fixture fix{make_ssl_config()};

   // Run the algorithm. No SSL stream reset is performed here
   auto act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_resolve);
   act = fix.fsm.resume(error_code(), resolver_data, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_connect);
   act = fix.fsm.resume(error_code(), endpoint, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::ssl_handshake);
   act = fix.fsm.resume(error::empty_field, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, error_code(error::empty_field));

   // The stream is marked as used
   BOOST_TEST(fix.st.ssl_stream_used);

   // Check logging
   fix.check_log({
      // clang-format off
      {logger::level::info, "Resolve results: 192.168.10.1:1234, 192.168.10.2:1235"},
      {logger::level::info, "Connected to 192.168.10.1:1234"                       },
      {logger::level::info, "Failed to perform SSL handshake: Expected field value is empty. [boost.redis:5]"},
      // clang-format on
   });
}

void test_ssl_handshake_timeout()
{
   // Setup
   fixture fix{make_ssl_config()};

   // Run the algorithm. Timeout = operation_aborted without the cancel type set
   auto act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_resolve);
   act = fix.fsm.resume(error_code(), resolver_data, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_connect);
   act = fix.fsm.resume(error_code(), endpoint, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::ssl_handshake);
   act = fix.fsm.resume(asio::error::operation_aborted, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, error_code(error::ssl_handshake_timeout));

   // The stream is marked as used
   BOOST_TEST(fix.st.ssl_stream_used);

   // Check logging
   fix.check_log({
      // clang-format off
      {logger::level::info, "Resolve results: 192.168.10.1:1234, 192.168.10.2:1235"},
      {logger::level::info, "Connected to 192.168.10.1:1234"                       },
      {logger::level::info, "Failed to perform SSL handshake: SSL handshake timeout. [boost.redis:20]"},
      // clang-format on
   });
}

void test_ssl_handshake_cancel()
{
   // Setup
   fixture fix{make_ssl_config()};

   // Run the algorithm. Cancel = operation_aborted with the cancel type set
   auto act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_resolve);
   act = fix.fsm.resume(error_code(), resolver_data, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_connect);
   act = fix.fsm.resume(error_code(), endpoint, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::ssl_handshake);
   act = fix.fsm.resume(asio::error::operation_aborted, fix.st, cancellation_type_t::terminal);
   BOOST_TEST_EQ(act, error_code(asio::error::operation_aborted));

   // The stream is marked as used
   BOOST_TEST(fix.st.ssl_stream_used);

   // Logging is system-dependent, so we don't check messages
   BOOST_TEST_EQ(fix.msgs.size(), 3u);
}

void test_ssl_handshake_cancel_edge()
{
   // Setup
   fixture fix{make_ssl_config()};

   // Run the algorithm. No error, but the cancel state is set
   auto act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_resolve);
   act = fix.fsm.resume(error_code(), resolver_data, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::tcp_connect);
   act = fix.fsm.resume(error_code(), endpoint, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::ssl_handshake);
   act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::terminal);
   BOOST_TEST_EQ(act, error_code(asio::error::operation_aborted));

   // The stream is marked as used
   BOOST_TEST(fix.st.ssl_stream_used);

   // Logging is system-dependent, so we don't check messages
   BOOST_TEST_EQ(fix.msgs.size(), 3u);
}

// UNIX connect errors
void test_unix_connect_error()
{
   // Setup
   fixture fix{make_unix_config()};

   // Run the algorithm
   auto act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::unix_socket_close);
   act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::unix_socket_connect);
   act = fix.fsm.resume(error::empty_field, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, error_code(error::empty_field));

   // Check logging
   fix.check_log({
      // clang-format off
      {logger::level::info, "Failed to connect to the server: Expected field value is empty. [boost.redis:5]"},
      // clang-format on
   });
}

void test_unix_connect_timeout()
{
   // Setup
   fixture fix{make_unix_config()};

   // Run the algorithm. Timeout = operation_aborted without a cancel state
   auto act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::unix_socket_close);
   act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::unix_socket_connect);
   act = fix.fsm.resume(asio::error::operation_aborted, fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, error_code(error::connect_timeout));

   // Check logging
   fix.check_log({
      // clang-format off
      {logger::level::info, "Failed to connect to the server: Connect timeout. [boost.redis:18]"},
      // clang-format on
   });
}

void test_unix_connect_cancel()
{
   // Setup
   fixture fix{make_unix_config()};

   // Run the algorithm. Cancel = operation_aborted with a cancel state
   auto act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::unix_socket_close);
   act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::unix_socket_connect);
   act = fix.fsm.resume(asio::error::operation_aborted, fix.st, cancellation_type_t::terminal);
   BOOST_TEST_EQ(act, error_code(asio::error::operation_aborted));

   // Logging is system-dependent
   BOOST_TEST_EQ(fix.msgs.size(), 1u);
}

void test_unix_connect_cancel_edge()
{
   // Setup
   fixture fix{make_unix_config()};

   // Run the algorithm. No error, but cancel state is set
   auto act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::unix_socket_close);
   act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::none);
   BOOST_TEST_EQ(act, connect_action_type::unix_socket_connect);
   act = fix.fsm.resume(error_code(), fix.st, cancellation_type_t::terminal);
   BOOST_TEST_EQ(act, error_code(asio::error::operation_aborted));

   // Logging is system-dependent
   BOOST_TEST_EQ(fix.msgs.size(), 1u);
}

}  // namespace

int main()
{
   test_tcp_success();
   test_tcp_tls_success();
   test_tcp_tls_success_reconnect();
   test_unix_success();
   test_unix_success_close_error();

   test_tcp_resolve_error();
   test_tcp_resolve_timeout();
   test_tcp_resolve_cancel();
   test_tcp_resolve_cancel_edge();

   test_tcp_connect_error();
   test_tcp_connect_timeout();
   test_tcp_connect_cancel();
   test_tcp_connect_cancel_edge();

   test_ssl_handshake_error();
   test_ssl_handshake_timeout();
   test_ssl_handshake_cancel();
   test_ssl_handshake_cancel_edge();

   test_unix_connect_error();
   test_unix_connect_timeout();
   test_unix_connect_cancel();
   test_unix_connect_cancel_edge();

   return boost::report_errors();
}
