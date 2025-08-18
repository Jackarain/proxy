#include <boost/asio/co_spawn.hpp>
#include <boost/asio/consign.hpp>

#include "common.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace net = boost::asio;

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

static std::string safe_getenv(const char* name, const char* default_value)
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
   cfg.max_read_size = 1000000;
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
