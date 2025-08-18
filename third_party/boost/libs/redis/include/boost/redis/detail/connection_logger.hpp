/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_CONNECTION_LOGGER_HPP
#define BOOST_REDIS_CONNECTION_LOGGER_HPP

#include <boost/redis/detail/reader_fsm.hpp>
#include <boost/redis/logger.hpp>
#include <boost/redis/response.hpp>

#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>

#include <string_view>

namespace boost::redis::detail {

// Wraps a logger and a string buffer for re-use, and provides
// utility functions to format the log messages that we use.
// The long-term trend will be moving most of this class to finite state
// machines as we write them
class connection_logger {
   logger logger_;
   std::string msg_;

public:
   connection_logger(logger&& logger) noexcept
   : logger_(std::move(logger))
   { }

   void reset(logger&& logger) { logger_ = std::move(logger); }

   void on_resolve(system::error_code const& ec, asio::ip::tcp::resolver::results_type const& res);
   void on_connect(system::error_code const& ec, asio::ip::tcp::endpoint const& ep);
   void on_connect(system::error_code const& ec, std::string_view unix_socket_ep);
   void on_ssl_handshake(system::error_code const& ec);
   void on_write(system::error_code const& ec, std::size_t n);
   void on_fsm_resume(reader_fsm::action const& action);
   void on_hello(system::error_code const& ec, generic_response const& resp);
   void log(logger::level lvl, std::string_view msg);
   void log(logger::level lvl, std::string_view op, system::error_code const& ec);
   void trace(std::string_view message) { log(logger::level::debug, message); }
   void trace(std::string_view op, system::error_code const& ec)
   {
      log(logger::level::debug, op, ec);
   }
};

}  // namespace boost::redis::detail

#endif  // BOOST_REDIS_LOGGER_HPP
