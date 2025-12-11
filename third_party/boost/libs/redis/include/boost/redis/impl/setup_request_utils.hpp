/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_SETUP_REQUEST_UTILS_HPP
#define BOOST_REDIS_SETUP_REQUEST_UTILS_HPP

#include <boost/redis/config.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/response.hpp>

namespace boost::redis::detail {

// Modifies config::setup to make a request suitable to be sent
// to the server using async_exec
inline void compose_setup_request(config& cfg)
{
   if (!cfg.use_setup) {
      // We're not using the setup request as-is, but should compose one based on
      // the values passed by the user
      auto& req = cfg.setup;
      req.clear();

      // Which parts of the command should we send?
      // Don't send AUTH if the user is the default and the password is empty.
      // Other users may have empty passwords.
      // Note that this is just an optimization.
      bool send_auth = !(
         cfg.username.empty() || (cfg.username == "default" && cfg.password.empty()));
      bool send_setname = !cfg.clientname.empty();

      // Gather everything we can in a HELLO command
      if (send_auth && send_setname)
         req.push("HELLO", "3", "AUTH", cfg.username, cfg.password, "SETNAME", cfg.clientname);
      else if (send_auth)
         req.push("HELLO", "3", "AUTH", cfg.username, cfg.password);
      else if (send_setname)
         req.push("HELLO", "3", "SETNAME", cfg.clientname);
      else
         req.push("HELLO", "3");

      // SELECT is independent of HELLO
      if (cfg.database_index && cfg.database_index.value() != 0)
         req.push("SELECT", cfg.database_index.value());
   }

   // In any case, the setup request should have the priority
   // flag set so it's executed before any other request.
   // The setup request should never be retried.
   request_access::set_priority(cfg.setup, true);
   cfg.setup.get_config().cancel_if_unresponded = true;
   cfg.setup.get_config().cancel_on_connection_lost = true;
}

}  // namespace boost::redis::detail

#endif  // BOOST_REDIS_RUNNER_HPP
