/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/connection.hpp>
#include <boost/redis/ignore.hpp>

#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>

#include <cerrno>
#include <cstddef>
#include <fstream>
#include <string>
#include <string_view>
#define BOOST_TEST_MODULE conn_tls
#include <boost/test/included/unit_test.hpp>

#include "common.hpp"

namespace net = boost::asio;
using namespace boost::redis;
using namespace std::chrono_literals;
using boost::system::error_code;

namespace {

// Loads the CA certificate that signed the certificate used by the server.
// Should be in /tmp/
std::string load_ca_certificate()
{
   auto ca_path = safe_getenv("BOOST_REDIS_CA_PATH", "/opt/ci-tls/ca.crt");
   std::ifstream f(ca_path);
   if (!f) {
      throw boost::system::system_error(
         errno,
         boost::system::system_category(),
         "Failed to open CA certificate file '" + ca_path + "'");
   }

   return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

config make_tls_config()
{
   config cfg;
   cfg.use_ssl = true;
   cfg.addr.host = get_server_hostname();
   cfg.addr.port = "16379";
   return cfg;
}

// Using the default TLS context allows establishing TLS connections and execute requests
BOOST_AUTO_TEST_CASE(exec_default_ssl_context)
{
   auto const cfg = make_tls_config();
   constexpr std::string_view ping_value = "Kabuf";

   request req;
   req.push("PING", ping_value);

   response<std::string> resp;

   net::io_context ioc;
   connection conn{ioc};

   // The custom server uses a certificate signed by a CA
   // that is not trusted by default - skip verification.
   conn.next_layer().set_verify_mode(net::ssl::verify_none);

   bool exec_finished = false, run_finished = false;

   conn.async_exec(req, resp, [&](error_code ec, std::size_t) {
      exec_finished = true;
      BOOST_TEST(ec == error_code());
      conn.cancel();
   });

   conn.async_run(cfg, {}, [&](error_code ec) {
      run_finished = true;
      BOOST_TEST(ec == net::error::operation_aborted);
   });

   ioc.run_for(test_timeout);

   BOOST_TEST(exec_finished);
   BOOST_TEST(run_finished);
   BOOST_TEST(std::get<0>(resp).value() == ping_value);
}

// Users can pass a custom context with TLS config
BOOST_AUTO_TEST_CASE(exec_custom_ssl_context)
{
   std::string ca_pem = load_ca_certificate();
   auto const cfg = make_tls_config();
   constexpr std::string_view ping_value = "Kabuf";

   request req;
   req.push("PING", ping_value);

   response<std::string> resp;

   net::io_context ioc;
   net::ssl::context ctx{net::ssl::context::tls_client};

   // Configure the SSL context to trust the CA that signed the server's certificate.
   // The test certificate uses "redis" as its common name, regardless of the actual server's hostname
   ctx.add_certificate_authority(net::buffer(ca_pem));
   ctx.set_verify_mode(net::ssl::verify_peer);
   ctx.set_verify_callback(net::ssl::host_name_verification("redis"));

   connection conn{ioc, std::move(ctx)};

   bool exec_finished = false, run_finished = false;

   conn.async_exec(req, resp, [&](error_code ec, std::size_t) {
      exec_finished = true;
      BOOST_TEST(ec == error_code());
      conn.cancel();
   });

   conn.async_run(cfg, {}, [&](error_code ec) {
      run_finished = true;
      BOOST_TEST(ec == net::error::operation_aborted);
   });

   ioc.run_for(test_timeout);

   BOOST_TEST(exec_finished);
   BOOST_TEST(run_finished);
   BOOST_TEST(std::get<0>(resp).value() == ping_value);
}

// After an error, a TLS connection can recover.
// Force an error using QUIT, then issue a regular request to verify that we could reconnect
BOOST_AUTO_TEST_CASE(reconnection)
{
   // Setup
   net::io_context ioc;
   net::steady_timer timer{ioc};
   connection conn{ioc};

   request ping_request;
   ping_request.push("PING", "some_value");
   ping_request.get_config().cancel_if_unresponded = false;
   ping_request.get_config().cancel_on_connection_lost = false;

   request quit_request;
   quit_request.push("QUIT");

   bool exec_finished = false, run_finished = false;

   // Run the connection
   conn.async_run(make_test_config(), [&](error_code ec) {
      run_finished = true;
      BOOST_TEST(ec == net::error::operation_aborted);
   });

   // The PING is the end of the callback chain
   auto ping_callback = [&](error_code ec, std::size_t) {
      exec_finished = true;
      BOOST_TEST(ec == error_code());
      conn.cancel();
   };

   auto quit_callback = [&](error_code ec, std::size_t) {
      BOOST_TEST(ec == error_code());
      conn.async_exec(ping_request, ignore, ping_callback);
   };

   conn.async_exec(quit_request, ignore, quit_callback);

   ioc.run_for(test_timeout);

   BOOST_TEST(exec_finished);
   BOOST_TEST(run_finished);
}

}  // namespace