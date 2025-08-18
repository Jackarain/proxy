//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include <boost/asio/awaitable.hpp>

#if defined(BOOST_ASIO_HAS_CO_AWAIT)

#include <boost/redis/connection.hpp>

#include <boost/asio/consign.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/this_coro.hpp>

#include <iostream>

namespace asio = boost::asio;
using boost::redis::request;
using boost::redis::response;
using boost::redis::config;
using boost::redis::logger;
using boost::redis::connection;

#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS

auto co_main(config cfg) -> asio::awaitable<void>
{
   // If unix_socket is set to a non-empty string, UNIX domain sockets will be used
   // instead of TCP. Set this value to the path where your server is listening.
   // UNIX domain socket connections work in the same way as TCP connections.
   cfg.unix_socket = "/tmp/redis-socks/redis.sock";

   auto conn = std::make_shared<connection>(co_await asio::this_coro::executor);
   conn->async_run(cfg, asio::consign(asio::detached, conn));

   request req;
   req.push("PING");

   response<std::string> resp;

   co_await conn->async_exec(req, resp);
   conn->cancel();

   std::cout << "Response: " << std::get<0>(resp).value() << std::endl;
}

#else

auto co_main(config) -> asio::awaitable<void>
{
   std::cout << "Sorry, your system does not support UNIX domain sockets\n";
   co_return;
}

#endif

#endif
