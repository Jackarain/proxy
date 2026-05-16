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
      case run_action_type::sentinel_resolve:      return "run_action_type::sentinel_resolve";
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

void test_config_error_unix_sentinel()
{
   // Setup
   config cfg;
   cfg.sentinel.addresses = {
      {"localhost", "26379"}
   };
   cfg.unix_socket = "/var/sock";
   fixture fix{std::move(cfg)};

   // Launching the operation fails immediately
   auto act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::immediate);
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, error_code(error::sentinel_unix_sockets_unsupported));

   // Log
   fix.check_log({
      {logger::level::err,
       "Invalid configuration: The configuration specified UNIX sockets with Sentinel, which is "
       "not supported. [boost.redis:28]"},
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

   // Log
   fix.check_log({
      // clang-format off
      {logger::level::info, "Trying to connect to Redis server at 127.0.0.1:6379 (TLS disabled)"  },
      {logger::level::err,  "Failed to connect to Redis server at 127.0.0.1:6379 (TLS disabled): Connect timeout. [boost.redis:18]"},
      {logger::level::info, "Trying to connect to Redis server at 127.0.0.1:6379 (TLS disabled)"  },
      {logger::level::info, "Connected to Redis server at 127.0.0.1:6379 (TLS disabled)"          },
      // clang-format on
   });
}

// Check logs for other transport types
void test_connect_error_ssl()
{
   // Setup
   fixture fix;
   fix.st.cfg.addr = {"my_hostname", "10000"};
   fix.st.cfg.use_ssl = true;

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

   // Log
   fix.check_log({
      // clang-format off
      {logger::level::info, "Trying to connect to Redis server at my_hostname:10000 (TLS enabled)"  },
      {logger::level::err,  "Failed to connect to Redis server at my_hostname:10000 (TLS enabled): Connect timeout. [boost.redis:18]"},
      {logger::level::info, "Trying to connect to Redis server at my_hostname:10000 (TLS enabled)"  },
      {logger::level::info, "Connected to Redis server at my_hostname:10000 (TLS enabled)"          },
      // clang-format on
   });
}

#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
void test_connect_error_unix()
{
   // Setup
   fixture fix;
   fix.st.cfg.unix_socket = "/tmp/sock";

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

   // Log
   fix.check_log({
      // clang-format off
      {logger::level::info, "Trying to connect to Redis server at '/tmp/sock'"  },
      {logger::level::err,  "Failed to connect to Redis server at '/tmp/sock': Connect timeout. [boost.redis:18]"},
      {logger::level::info, "Trying to connect to Redis server at '/tmp/sock'"  },
      {logger::level::info, "Connected to Redis server at '/tmp/sock'"          },
      // clang-format on
   });
}
#endif

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

   // Log
   fix.check_log({
      // clang-format off
      {logger::level::info, "Trying to connect to Redis server at 127.0.0.1:6379 (TLS disabled)"  },
      {logger::level::err,  "Failed to connect to Redis server at 127.0.0.1:6379 (TLS disabled): Connect timeout. [boost.redis:18]"},
      // clang-format on
   });
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

   // Log
   fix.check_log({
      {logger::level::info,  "Trying to connect to Redis server at 127.0.0.1:6379 (TLS disabled)"},
      {logger::level::debug, "Run: cancelled (1)"                                                }
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

   // Log
   fix.check_log({
      {logger::level::info,  "Trying to connect to Redis server at 127.0.0.1:6379 (TLS disabled)"},
      {logger::level::debug, "Run: cancelled (1)"                                                }
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

   // Log
   fix.check_log({
      {logger::level::info, "Trying to connect to Redis server at 127.0.0.1:6379 (TLS disabled)"},
      {logger::level::info, "Connected to Redis server at 127.0.0.1:6379 (TLS disabled)"        },
      {logger::level::info, "Trying to connect to Redis server at 127.0.0.1:6379 (TLS disabled)"},
      {logger::level::info, "Connected to Redis server at 127.0.0.1:6379 (TLS disabled)"        },
   });
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

   // Log
   fix.check_log({
      {logger::level::info, "Trying to connect to Redis server at 127.0.0.1:6379 (TLS disabled)"},
      {logger::level::info, "Connected to Redis server at 127.0.0.1:6379 (TLS disabled)"        },
   });
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

   // Log
   fix.check_log({
      {logger::level::info,  "Trying to connect to Redis server at 127.0.0.1:6379 (TLS disabled)"},
      {logger::level::info,  "Connected to Redis server at 127.0.0.1:6379 (TLS disabled)"        },
      {logger::level::debug, "Run: cancelled (2)"                                                }
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

   // Log
   fix.check_log({
      {logger::level::info,  "Trying to connect to Redis server at 127.0.0.1:6379 (TLS disabled)"},
      {logger::level::info,  "Connected to Redis server at 127.0.0.1:6379 (TLS disabled)"        },
      {logger::level::debug, "Run: cancelled (2)"                                                }
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

   // Log
   fix.check_log({
      {logger::level::info,  "Trying to connect to Redis server at 127.0.0.1:6379 (TLS disabled)"},
      {logger::level::info,  "Connected to Redis server at 127.0.0.1:6379 (TLS disabled)"        },
      {logger::level::debug, "Run: cancelled (3)"                                                }
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

   // Log
   fix.check_log({
      {logger::level::info,  "Trying to connect to Redis server at 127.0.0.1:6379 (TLS disabled)"},
      {logger::level::info,  "Connected to Redis server at 127.0.0.1:6379 (TLS disabled)"        },
      {logger::level::debug, "Run: cancelled (3)"                                                }
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

   // Log
   fix.check_log({
      // clang-format off
      {logger::level::info,  "Trying to connect to Redis server at 127.0.0.1:6379 (TLS disabled)"  },
      {logger::level::err,   "Failed to connect to Redis server at 127.0.0.1:6379 (TLS disabled): Connect timeout. [boost.redis:18]"},
      {logger::level::info,  "Trying to connect to Redis server at 127.0.0.1:6379 (TLS disabled)"  },
      {logger::level::info,  "Connected to Redis server at 127.0.0.1:6379 (TLS disabled)"          },
      {logger::level::info,  "Trying to connect to Redis server at 127.0.0.1:6379 (TLS disabled)"},
      {logger::level::info,  "Connected to Redis server at 127.0.0.1:6379 (TLS disabled)"        },
      {logger::level::debug, "Run: cancelled (2)"                                                }  // clang-format on
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
   BOOST_TEST_EQ(fix.st.setup_req.payload(), expected_setup);

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
   BOOST_TEST_EQ(fix.st.setup_req.payload(), expected_setup);
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
      // clang-format off
      {logger::level::info, "Trying to connect to Redis server at 127.0.0.1:6379 (TLS disabled)"  },
      {logger::level::info, "Connected to Redis server at 127.0.0.1:6379 (TLS disabled)"          },
      {logger::level::info, "Setup request execution: success"},
      // clang-format on
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

   // Log
   fix.check_log({
      // clang-format off
      {logger::level::info, "Trying to connect to Redis server at 127.0.0.1:6379 (TLS disabled)"  },
      {logger::level::info, "Connected to Redis server at 127.0.0.1:6379 (TLS disabled)"          },
      // clang-format on
   });
}

// A server error would cause the reader to exit
void test_setup_request_server_error()
{
   // Setup
   fixture fix;
   fix.st.diagnostic = "leftover";  // simulate a leftover from previous runs
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
      {logger::level::info, "Trying to connect to Redis server at 127.0.0.1:6379 (TLS disabled)"},
      {logger::level::info, "Connected to Redis server at 127.0.0.1:6379 (TLS disabled)"        },
      {logger::level::err,  "Setup request execution failed: ERR: wrong command"                }
   });
}

// If the setup request finishes with another error (e.g. network error), we log it
void test_setup_request_other_error()
{
   // Setup
   fixture fix;
   fix.st.diagnostic = "leftover";  // simulate a leftover from previous runs
   fix.st.cfg.setup.clear();
   fix.st.cfg.setup.push("HELLO", 3);
   fix.st.cfg.reconnect_wait_interval = 0s;

   // Run the operation. We connect and launch the tasks
   auto act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::connect);
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::parallel_group);

   // At this point, the setup request should be already queued.
   // Simulate the writer
   BOOST_TEST_EQ(fix.st.mpx.prepare_write(), 1u);
   BOOST_TEST(fix.st.mpx.commit_write(fix.st.mpx.get_write_buffer().size()));

   // Simulate a read error
   read(fix.st.mpx, "*malformed\r\n");
   error_code ec;
   auto res = fix.st.mpx.consume(ec);
   BOOST_TEST_EQ(ec, error::not_a_number);
   BOOST_TEST(res.first == detail::consume_result::got_response);

   // This will cause the writer to exit
   act = fix.fsm.resume(fix.st, error::not_a_number, cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::cancel_receive);
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, error_code(error::not_a_number));

   // Check log
   fix.check_log({
      // clang-format off
      {logger::level::info, "Trying to connect to Redis server at 127.0.0.1:6379 (TLS disabled)"},
      {logger::level::info, "Connected to Redis server at 127.0.0.1:6379 (TLS disabled)"        },
      {logger::level::err,  "Setup request execution failed: Can't convert string to number (maybe forgot to upgrade to RESP3?). [boost.redis:2]" },
      // clang-format on
   });
}

// When using Sentinel, reconnection works normally
void test_sentinel_reconnection()
{
   // Setup
   fixture fix;
   fix.st.cfg.sentinel.addresses = {
      {"localhost", "26379"}
   };

   // Resolve succeeds, and a connection is attempted
   auto act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::sentinel_resolve);
   fix.st.cfg.addr = {"host1", "1000"};
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::connect);

   // This errors, so we sleep and resolve again
   act = fix.fsm.resume(fix.st, error::connect_timeout, cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::wait_for_reconnection);
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::sentinel_resolve);
   fix.st.cfg.addr = {"host2", "2000"};
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::connect);
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::parallel_group);

   // Sentinel involves always a setup request containing the role check. Run it.
   BOOST_TEST_EQ(fix.st.mpx.prepare_write(), 1u);
   BOOST_TEST(fix.st.mpx.commit_write(fix.st.mpx.get_write_buffer().size()));
   read(fix.st.mpx, "*1\r\n$6\r\nmaster\r\n");
   error_code ec;
   auto res = fix.st.mpx.consume(ec);
   BOOST_TEST_EQ(ec, error_code());
   BOOST_TEST(res.first == detail::consume_result::got_response);

   // The parallel group errors, so we sleep and resolve again
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::cancel_receive);
   act = fix.fsm.resume(fix.st, error::write_timeout, cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::wait_for_reconnection);
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::sentinel_resolve);
   fix.st.cfg.addr = {"host3", "3000"};
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::connect);

   // Cancel
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::terminal);
   BOOST_TEST_EQ(act, error_code(asio::error::operation_aborted));

   // Log
   fix.check_log({
      // clang-format off
      {logger::level::info,  "Trying to connect to Redis server at host1:1000 (TLS disabled)"},
      {logger::level::err,   "Failed to connect to Redis server at host1:1000 (TLS disabled): Connect timeout. [boost.redis:18]"},
      {logger::level::info,  "Trying to connect to Redis server at host2:2000 (TLS disabled)"},
      {logger::level::info,  "Connected to Redis server at host2:2000 (TLS disabled)"},
      {logger::level::info,  "Setup request execution: success"},
      {logger::level::info,  "Trying to connect to Redis server at host3:3000 (TLS disabled)"},
      {logger::level::debug, "Run: cancelled (1)"},
      // clang-format on
   });
}

// If the Sentinel resolve operation errors, we try again
void test_sentinel_resolve_error()
{
   // Setup
   fixture fix;
   fix.st.cfg.sentinel.addresses = {
      {"localhost", "26379"}
   };

   // Start the Sentinel resolve operation
   auto act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::sentinel_resolve);

   // It fails with an error, so we go to sleep
   act = fix.fsm.resume(fix.st, error::sentinel_resolve_failed, cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::wait_for_reconnection);

   // Retrying it succeeds
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::sentinel_resolve);
   fix.st.cfg.addr = {"myhost", "10000"};
   act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::connect);

   // Log
   fix.check_log({
      {logger::level::info, "Trying to connect to Redis server at myhost:10000 (TLS disabled)"},
   });
}

// The reconnection setting affects Sentinel reconnection, too
void test_sentinel_resolve_error_no_reconnect()
{
   // Setup
   fixture fix{config_no_reconnect()};
   fix.st.cfg.sentinel.addresses = {
      {"localhost", "26379"}
   };

   // Start the Sentinel resolve operation
   auto act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::sentinel_resolve);

   // It fails with an error, so we exit
   act = fix.fsm.resume(fix.st, error::sentinel_resolve_failed, cancellation_type_t::none);
   BOOST_TEST_EQ(act, error_code(error::sentinel_resolve_failed));

   // Log
   fix.check_log({});
}

void test_sentinel_resolve_cancel()
{
   // Setup
   fixture fix;
   fix.st.cfg.sentinel.addresses = {
      {"localhost", "26379"}
   };

   // Start the Sentinel resolve operation
   auto act = fix.fsm.resume(fix.st, error_code(), cancellation_type_t::none);
   BOOST_TEST_EQ(act, run_action_type::sentinel_resolve);
   act = fix.fsm.resume(fix.st, asio::error::operation_aborted, cancellation_type_t::terminal);
   BOOST_TEST_EQ(act, error_code(asio::error::operation_aborted));

   // Log
   fix.check_log({
      {logger::level::debug, "Run: cancelled (4)"},
   });
}

}  // namespace

int main()
{
#ifndef BOOST_ASIO_HAS_LOCAL_SOCKETS
   test_config_error_unix();
#endif
   test_config_error_unix_ssl();
   test_config_error_unix_sentinel();

   test_connect_error();
   test_connect_error_ssl();
#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
   test_connect_error_unix();
#endif
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
   test_setup_request_other_error();

   test_sentinel_reconnection();
   test_sentinel_resolve_error();
   test_sentinel_resolve_error_no_reconnect();
   test_sentinel_resolve_cancel();

   return boost::report_errors();
}
