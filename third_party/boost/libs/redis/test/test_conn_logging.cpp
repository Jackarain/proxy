//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/config.hpp>
#include <boost/redis/connection.hpp>
#include <boost/redis/logger.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/system/error_code.hpp>

#include "common.hpp"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

using boost::system::error_code;
namespace net = boost::asio;
using namespace boost::redis;

namespace {

// user tests
//     logging can be disabled
//     logging can be changed verbosity

template <class Conn>
void run_with_invalid_config(net::io_context& ioc, Conn& conn)
{
   config cfg;
   cfg.use_ssl = true;
   cfg.unix_socket = "/tmp/sock";
   conn.async_run(cfg, [](error_code ec) {
      BOOST_TEST_NE(ec, error_code());
   });
   ioc.run_for(test_timeout);
}

template <class Conn>
void test_connection_constructor_executor_1()
{
   // Setup
   net::io_context ioc;
   std::vector<std::string> messages;
   logger lgr(logger::level::info, [&](logger::level, std::string_view msg) {
      messages.emplace_back(msg);
   });
   Conn conn{ioc.get_executor(), std::move(lgr)};

   // Produce some logging
   run_with_invalid_config(ioc, conn);

   // Some logging was produced
   BOOST_TEST_EQ(messages.size(), 1u);
}

template <class Conn>
void test_connection_constructor_context_1()
{
   // Setup
   net::io_context ioc;
   std::vector<std::string> messages;
   logger lgr(logger::level::info, [&](logger::level, std::string_view msg) {
      messages.emplace_back(msg);
   });
   Conn conn{ioc, std::move(lgr)};

   // Produce some logging
   run_with_invalid_config(ioc, conn);

   // Some logging was produced
   BOOST_TEST_EQ(messages.size(), 1u);
}

template <class Conn>
void test_connection_constructor_executor_2()
{
   // Setup
   net::io_context ioc;
   std::vector<std::string> messages;
   logger lgr(logger::level::info, [&](logger::level, std::string_view msg) {
      messages.emplace_back(msg);
   });
   Conn conn{
      ioc.get_executor(),
      net::ssl::context{net::ssl::context::tlsv12_client},
      std::move(lgr)};

   // Produce some logging
   run_with_invalid_config(ioc, conn);

   // Some logging was produced
   BOOST_TEST_EQ(messages.size(), 1u);
}

template <class Conn>
void test_connection_constructor_context_2()
{
   // Setup
   net::io_context ioc;
   std::vector<std::string> messages;
   logger lgr(logger::level::info, [&](logger::level, std::string_view msg) {
      messages.emplace_back(msg);
   });
   Conn conn{ioc, net::ssl::context{net::ssl::context::tlsv12_client}, std::move(lgr)};

   // Produce some logging
   run_with_invalid_config(ioc, conn);

   // Some logging was produced
   BOOST_TEST_EQ(messages.size(), 1u);
}

void test_disable_logging()
{
   // Setup
   net::io_context ioc;
   std::vector<std::string> messages;
   logger lgr(logger::level::disabled, [&](logger::level, std::string_view msg) {
      messages.emplace_back(msg);
   });
   connection conn{ioc, std::move(lgr)};

   // Produce some logging
   run_with_invalid_config(ioc, conn);

   // Some logging was produced
   BOOST_TEST_EQ(messages.size(), 0u);
}

}  // namespace

int main()
{
   // basic_connection
   using basic_conn_t = basic_connection<net::io_context::executor_type>;
   test_connection_constructor_executor_1<basic_conn_t>();
   test_connection_constructor_executor_2<basic_conn_t>();
   test_connection_constructor_context_1<basic_conn_t>();
   test_connection_constructor_context_2<basic_conn_t>();

   // connection
   test_connection_constructor_executor_1<connection>();
   test_connection_constructor_executor_2<connection>();
   test_connection_constructor_context_1<connection>();
   test_connection_constructor_context_2<connection>();

   test_disable_logging();

   return boost::report_errors();
}
