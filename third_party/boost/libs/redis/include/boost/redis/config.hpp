/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_CONFIG_HPP
#define BOOST_REDIS_CONFIG_HPP

#include <boost/redis/request.hpp>

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

   /** @brief Username used for authentication during connection establishment.
    * 
    * If @ref use_setup is false (the default), during connection establishment,
    * authentication is performed by sending a `HELLO` command.
    * This field contains the username to employ.
    *
    * If the username equals the literal `"default"` (the default)
    * and no password is specified, the `HELLO` command is sent
    * without authentication parameters.
    */
   std::string username = "default";

   /** @brief Password used for authentication during connection establishment.
    * 
    * If @ref use_setup is false (the default), during connection establishment,
    * authentication is performed by sending a `HELLO` command.
    * This field contains the password to employ.
    *
    * If the username equals the literal `"default"` (the default)
    * and no password is specified, the `HELLO` command is sent
    * without authentication parameters.
    */
   std::string password;

   /** @brief Client name parameter to use during connection establishment.
    * 
    * If @ref use_setup is false (the default), during connection establishment,
    * a `HELLO` command is sent. If this field is not empty, the `HELLO` command
    * will contain a `SETNAME` subcommand containing this value.
    */
   std::string clientname = "Boost.Redis";

   /** @brief Database index to pass to the `SELECT` command during connection establishment.
    * 
    * If @ref use_setup is false (the default), and this field is set to a
    * non-empty optional, and its value is different than zero,
    * a `SELECT` command will be issued during connection establishment to set the logical
    * database index. By default, no `SELECT` command is sent.
    */
   std::optional<int> database_index = 0;

   /// Message used by `PING` commands sent by the health checker.
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
    *  Set to zero to disable health-checks.
    *
    * When this value is set to a non-zero duration, @ref basic_connection::async_run
    * will issue `PING` commands whenever no command is sent to the server for more
    * than `health_check_interval`. You can configure the message passed to the `PING`
    * command using @ref health_check_id.
    *
    * Enabling health checks also sets timeouts to individual network
    * operations. The connection is considered dead if:
    *
    * @li No byte can be written to the server after `health_check_interval`.
    * @li No byte is read from the server after `2 * health_check_interval`.
    *
    * If the health checker finds that the connection is unresponsive, it will be closed,
    * and a reconnection will be triggered, as if a network error had occurred.
    *
    * The exact timeout values are *not* part of the interface, and might change
    * in future versions.
    */
   std::chrono::steady_clock::duration health_check_interval = std::chrono::seconds{2};

   /** @brief Time span to wait between successive connection retries.
    *  Set to zero to disable reconnection.
    */
   std::chrono::steady_clock::duration reconnect_wait_interval = std::chrono::seconds{1};

   /** @brief Maximum size of the socket read-buffer in bytes.
    *  
    *  Sets a limit on how much data is allowed to be read into the
    *  read buffer. It can be used to prevent DDOS.
    */
   std::size_t max_read_size = (std::numeric_limits<std::size_t>::max)();

   /** @brief Grow size of the read buffer.
    *
    * The size by which the read buffer grows when more space is
    * needed. This can help avoiding some memory allocations. Once the
    * maximum size is reached no more memory allocations are made
    * since the buffer is reused.
    */
   std::size_t read_buffer_append_size = 4096;

   /** @brief Enables using a custom requests during connection establishment.
    * 
    * If set to true, the @ref setup member will be sent to the server immediately after
    * connection establishment. Every time a reconnection happens, the setup
    * request will be executed before any other request.
    * It can be used to perform authentication,
    * subscribe to channels or select a database index.
    *
    * When set to true, *the custom setup request replaces the built-in HELLO
    * request generated by the library*. The @ref username, @ref password,
    * @ref clientname and @ref database_index fields *will be ignored*.
    *
    * By default, @ref setup contains a `"HELLO 3"` command, which upgrades the
    * protocol to RESP3. You might modify this request as you like,
    * but you should ensure that the resulting connection uses RESP3.
    *
    * To prevent sending any setup request at all, set this field to true
    * and @ref setup to an empty request. This can be used to interface with
    * systems that don't support `HELLO`.
    *
    * By default, this field is false, and @ref setup will not be used.
    */
   bool use_setup = false;

   /** @brief Request to be executed after connection establishment.
    *
    * This member is only used if @ref use_setup is `true`. Please consult
    * @ref use_setup docs for more info.
    *
    * By default, `setup` contains a `"HELLO 3"` command.
    */
   request setup = detail::make_hello_request();
};

}  // namespace boost::redis

#endif  // BOOST_REDIS_CONFIG_HPP
