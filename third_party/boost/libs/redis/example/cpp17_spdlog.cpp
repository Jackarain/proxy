//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/connection.hpp>
#include <boost/redis/logger.hpp>

#include <boost/asio/detached.hpp>
#include <boost/system/error_code.hpp>

#include <cstddef>
#include <iostream>
#include <spdlog/spdlog.h>
#include <string_view>

namespace asio = boost::asio;
namespace redis = boost::redis;

// Maps a Boost.Redis log level to a spdlog log level
static spdlog::level::level_enum to_spdlog_level(redis::logger::level lvl)
{
   switch (lvl) {
      // spdlog doesn't include the emerg and alert syslog levels,
      // so we convert them to the highest supported level.
      // Similarly, notice is similar to info
      case redis::logger::level::emerg:
      case redis::logger::level::alert:
      case redis::logger::level::crit:    return spdlog::level::critical;
      case redis::logger::level::err:     return spdlog::level::err;
      case redis::logger::level::warning: return spdlog::level::warn;
      case redis::logger::level::notice:
      case redis::logger::level::info:    return spdlog::level::info;
      case redis::logger::level::debug:
      default:                            return spdlog::level::debug;
   }
}

// This function glues Boost.Redis logging and spdlog.
// It should have the signature shown here. It will be invoked
// by Boost.Redis whenever a message is to be logged.
static void do_log(redis::logger::level level, std::string_view msg)
{
   spdlog::log(to_spdlog_level(level), "(Boost.Redis) {}", msg);
}

auto main(int argc, char* argv[]) -> int
{
   if (argc != 3) {
      std::cerr << "Usage: " << argv[0] << " <server-host> <server-port>\n";
      exit(1);
   }

   try {
      // Create an execution context, required to create any I/O objects
      asio::io_context ioc;

      // Create a connection to connect to Redis, and pass it a custom logger.
      // Boost.Redis will call do_log whenever it needs to log a message.
      // Note that the function will only be called for messages with level >= info
      // (i.e. filtering is done by Boost.Redis).
      redis::connection conn{
         ioc,
         redis::logger{redis::logger::level::info, do_log}
      };

      // Configuration to connect to the server
      redis::config cfg;
      cfg.addr.host = argv[1];
      cfg.addr.port = argv[2];

      // Run the connection with the specified configuration.
      // This will establish the connection and keep it healthy
      conn.async_run(cfg, asio::detached);

      // Execute a request
      redis::request req;
      req.push("PING", "Hello world");

      redis::response<std::string> resp;

      conn.async_exec(req, resp, [&](boost::system::error_code ec, std::size_t /* bytes_read*/) {
         if (ec) {
            spdlog::error("Request failed: {}", ec.what());
            exit(1);
         } else {
            spdlog::info("PING: {}", std::get<0>(resp).value());
         }
         conn.cancel();
      });

      // Actually run our example. Nothing will happen until we call run()
      ioc.run();

   } catch (std::exception const& e) {
      spdlog::error("Error: {}", e.what());
      return 1;
   }
}
