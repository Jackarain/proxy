//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/config.hpp>

#include <boost/config.hpp>

#if !defined(__cpp_designated_initializers) || (__cpp_designated_initializers < 201707L)

#include <boost/config/pragma_message.hpp>

BOOST_PRAGMA_MESSAGE("test_config skipped because designated initializers are not supported");

int main() { }

#else

#include <boost/core/lightweight_test.hpp>

using namespace boost::redis;

namespace {

// A config object can be created using designated initializers.
// No initializer is missing
void test_designated_initializers_config()
{
   // Most members have initializers
   config cfg1{
      .addr = {"127.0.0.1", "2000"},
      .use_setup = true,
   };

   // The ones included above have, too
   config cfg2{.unix_socket = "/tmp/sock"};

   // Sentinel config has them, too
   sentinel_config sent_cfg{.addresses = {{"127.0.0.1", "1000"}}};
}

}  // namespace

int main()
{
   test_designated_initializers_config();

   return boost::report_errors();
}

#endif
