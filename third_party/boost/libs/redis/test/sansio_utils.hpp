/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_TEST_SANSIO_UTILS_HPP
#define BOOST_REDIS_TEST_SANSIO_UTILS_HPP

#include <boost/redis/logger.hpp>

#include <boost/assert/source_location.hpp>

#include <chrono>
#include <initializer_list>
#include <string>
#include <string_view>

namespace boost::redis::detail {

class multiplexer;

// Read data into the multiplexer with the following steps
//
//   1. prepare_read
//   2. get_read_buffer
//   3. Copy data in the buffer from 2.
//   4. commit_read;
//
// This is used in the multiplexer tests.
void read(multiplexer& mpx, std::string_view data);

// Utilities for checking logs
struct log_message {
   logger::level lvl;
   std::string msg;
};

struct log_fixture {
   std::vector<log_message> msgs;

   void check_log(
      std::initializer_list<const log_message> expected,
      source_location loc = BOOST_CURRENT_LOCATION) const;
   logger make_logger();
};

constexpr auto to_milliseconds(std::chrono::steady_clock::duration d)
{
   return std::chrono::duration_cast<std::chrono::milliseconds>(d).count();
}

}  // namespace boost::redis::detail

#endif  // BOOST_REDIS_TEST_SANSIO_UTILS_HPP
