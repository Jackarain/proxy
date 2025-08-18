#pragma once

#include <boost/redis/connection.hpp>
#include <boost/redis/operation.hpp>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/system/error_code.hpp>

#include <chrono>
#include <memory>

// The timeout for tests involving communication to a real server.
// Some tests use a longer timeout by multiplying this value by some
// integral number.
inline constexpr std::chrono::seconds test_timeout{30};

#ifdef BOOST_ASIO_HAS_CO_AWAIT
inline auto redir(boost::system::error_code& ec)
{
   return boost::asio::redirect_error(boost::asio::use_awaitable, ec);
}
void run_coroutine_test(
   boost::asio::awaitable<void>,
   std::chrono::steady_clock::duration timeout = test_timeout);
#endif  // BOOST_ASIO_HAS_CO_AWAIT

boost::redis::config make_test_config();
std::string get_server_hostname();

void run(
   std::shared_ptr<boost::redis::connection> conn,
   boost::redis::config cfg = make_test_config(),
   boost::system::error_code ec = boost::asio::error::operation_aborted,
   boost::redis::operation op = boost::redis::operation::receive);
