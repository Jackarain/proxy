/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_CONFIG_HPP
#define BOOST_REDIS_CONFIG_HPP

#include <chrono>
#include <limits>
#include <optional>
#include <string>

namespace boost::redis {

/// Address of a Redis server.
struct address {
   /// Redis host.
   std::string host = "127.0.0.1";
   /// Redis port.
   std::string port = "6379";
};

/// Configure parameters used by the connection classes.
struct config {
   /// Uses SSL instead of a plain connection.
   bool use_ssl = false;

   /// For TCP connections, hostname and port of the Redis server.
   address addr = address{"127.0.0.1", "6379"};

   /**
    * @brief The UNIX domain socket path where the server is listening.
    *
    * If non-empty, communication with the server will happen using
    * UNIX domain sockets, and @ref addr will be ignored.
    * UNIX domain sockets can't be used with SSL: if `unix_socket` is non-empty,
    * @ref use_ssl must be `false`.
    */
   std::string unix_socket;

   /** @brief Username passed to the `HELLO` command.
    * If left empty `HELLO` will be sent without authentication parameters.
    */
   std::string username = "default";

   /** @brief Password passed to the
    * `HELLO` command.  If left
    * empty `HELLO` will be sent without authentication parameters.
    */
   std::string password;

   /// Client name parameter of the `HELLO` command.
   std::string clientname = "Boost.Redis";

   /// Database that will be passed to the `SELECT` command.
   std::optional<int> database_index = 0;

   /// Message used by the health-checker in @ref boost::redis::basic_connection::async_run.
   std::string health_check_id = "Boost.Redis";

   /**
    * @brief (Deprecated) Sets the logger prefix, a string printed before log messages.
    * 
    * Setting a prefix in this struct is deprecated. If you need to change how log messages
    * look like, please construct a logger object passing a formatting function, and use that
    * logger in connection's constructor. This member will be removed in subsequent releases.
    */
   std::string log_prefix = "(Boost.Redis) ";

   /// Time span that the resolve operation is allowed to elapse.
   std::chrono::steady_clock::duration resolve_timeout = std::chrono::seconds{10};

   /// Time span that the connect operation is allowed to elapse.
   std::chrono::steady_clock::duration connect_timeout = std::chrono::seconds{10};

   /// Time span that the SSL handshake operation is allowed to elapse.
   std::chrono::steady_clock::duration ssl_handshake_timeout = std::chrono::seconds{10};

   /** @brief Time span between successive health checks.
    *  Set to zero to disable health-checks pass zero as duration.
    */
   std::chrono::steady_clock::duration health_check_interval = std::chrono::seconds{2};

   /** @brief Time span to wait between successive connection retries.
    *  Set to zero to disable reconnection.
    */
   std::chrono::steady_clock::duration reconnect_wait_interval = std::chrono::seconds{1};

   /** @brief Maximum size of a socket read, in bytes.
    *  
    *  Sets a limit on how much data is allowed to be read into the
    *  read buffer. It can be used to prevent DDOS.
    */
   std::size_t max_read_size = (std::numeric_limits<std::size_t>::max)();
};

}  // namespace boost::redis

#endif  // BOOST_REDIS_CONFIG_HPP
