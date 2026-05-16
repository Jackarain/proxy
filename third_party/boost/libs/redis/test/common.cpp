#include <boost/redis/config.hpp>
#include <boost/redis/ignore.hpp>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/consign.hpp>
#include <boost/core/lightweight_test.hpp>

#include "common.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace net = boost::asio;
using namespace std::chrono_literals;

struct run_callback {
   std::shared_ptr<boost::redis::connection> conn;
   boost::redis::operation op;
   boost::system::error_code expected;

   void operator()(boost::system::error_code const& ec) const
   {
      std::cout << "async_run: " << ec.message() << std::endl;
      conn->cancel(op);
   }
};

void run(
   std::shared_ptr<boost::redis::connection> conn,
   boost::redis::config cfg,
   boost::system::error_code ec,
   boost::redis::operation op)
{
   conn->async_run(cfg, run_callback{conn, op, ec});
}

std::string safe_getenv(const char* name, const char* default_value)
{
   // MSVC doesn't like getenv
#ifdef BOOST_MSVC
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
   const char* res = std::getenv(name);
#ifdef BOOST_MSVC
#pragma warning(pop)
#endif
   return res ? res : default_value;
}

std::string get_server_hostname() { return safe_getenv("BOOST_REDIS_TEST_SERVER", "localhost"); }

boost::redis::config make_test_config()
{
   boost::redis::config cfg;
   cfg.addr.host = get_server_hostname();
   cfg.reconnect_wait_interval = 50ms;  // make tests involving reconnection faster
   return cfg;
}

#ifdef BOOST_ASIO_HAS_CO_AWAIT
void run_coroutine_test(net::awaitable<void> op, std::chrono::steady_clock::duration timeout)
{
   net::io_context ioc;
   bool finished = false;
   net::co_spawn(ioc, std::move(op), [&finished](std::exception_ptr p) {
      if (p)
         std::rethrow_exception(p);
      finished = true;
   });
   ioc.run_for(timeout);
   if (!finished)
      throw std::runtime_error("Coroutine test did not finish");
}
#endif  // BOOST_ASIO_HAS_CO_AWAIT

// Finds a value in the output of the CLIENT INFO command
// format: key1=value1 key2=value2
std::string_view find_client_info(std::string_view client_info, std::string_view key)
{
   std::string prefix{key};
   prefix += '=';

   auto const pos = client_info.find(prefix);
   if (pos == std::string_view::npos)
      return {};
   auto const pos_begin = pos + prefix.size();
   auto const pos_end = client_info.find(' ', pos_begin);
   return client_info.substr(pos_begin, pos_end - pos_begin);
}

void create_user(std::string_view port, std::string_view username, std::string_view password)
{
   // Setup
   net::io_context ioc;
   boost::redis::connection conn{ioc};

   boost::redis::config cfg;
   cfg.addr.port = port;

   // Enable the user and grant them permissions on everything
   boost::redis::request req;
   req.push("ACL", "SETUSER", username, "on", ">" + std::string(password), "~*", "&*", "+@all");

   bool run_finished = false, exec_finished = false;

   conn.async_run(cfg, [&](boost::system::error_code ec) {
      run_finished = true;
      BOOST_TEST_EQ(ec, net::error::operation_aborted);
   });

   conn.async_exec(req, boost::redis::ignore, [&](boost::system::error_code ec, std::size_t) {
      exec_finished = true;
      BOOST_TEST_EQ(ec, boost::system::error_code());
      conn.cancel();
   });

   ioc.run_for(test_timeout);

   BOOST_TEST(run_finished);
   BOOST_TEST(exec_finished);
}

boost::redis::logger make_string_logger(std::string& to)
{
   return {
      boost::redis::logger::level::info,
      [&to](boost::redis::logger::level, std::string_view msg) {
         to += msg;
         to += '\n';
      }};
}
