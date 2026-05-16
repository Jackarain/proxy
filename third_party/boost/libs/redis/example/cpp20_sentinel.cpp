//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/connection.hpp>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/consign.hpp>
#include <boost/asio/detached.hpp>

#include <iostream>

#if defined(BOOST_ASIO_HAS_CO_AWAIT)

namespace asio = boost::asio;
using boost::redis::request;
using boost::redis::response;
using boost::redis::config;
using boost::redis::connection;

// Called from the main function (see main.cpp)
auto co_main(config cfg) -> asio::awaitable<void>
{
   // Boost.Redis has built-in support for Sentinel deployments.
   // To enable it, set the fields in config shown here.
   // sentinel.addresses should contain a list of (hostname, port) pairs
   // where Sentinels are listening. IPs can also be used.
   cfg.sentinel.addresses = {
      {"localhost", "26379"},
      {"localhost", "26380"},
      {"localhost", "26381"},
   };

   // Set master_name to the identifier that you configured
   // in the "sentinel monitor" statement of your sentinel.conf file
   cfg.sentinel.master_name = "mymaster";

   // async_run will contact the Sentinels, obtain the master address,
   // connect to it and keep the connection healthy. If a failover happens,
   // the address will be resolved again and the new elected master will be contacted.
   auto conn = std::make_shared<connection>(co_await asio::this_coro::executor);
   conn->async_run(cfg, asio::consign(asio::detached, conn));

   // You can now use the connection normally, as you would use a connection to a single master.
   request req;
   req.push("PING", "Hello world");
   response<std::string> resp;

   // Execute the request.
   co_await conn->async_exec(req, resp);
   conn->cancel();

   std::cout << "PING: " << std::get<0>(resp).value() << std::endl;
}

#endif  // defined(BOOST_ASIO_HAS_CO_AWAIT)
