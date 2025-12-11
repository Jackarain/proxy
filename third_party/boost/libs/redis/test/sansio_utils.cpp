/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/detail/multiplexer.hpp>

#include <boost/core/ignore_unused.hpp>
#include <boost/core/lightweight_test.hpp>

#include "sansio_utils.hpp"

#include <initializer_list>
#include <iostream>
#include <ostream>

using namespace boost::redis;

static constexpr const char* to_string(logger::level lvl)
{
   switch (lvl) {
      case logger::level::disabled: return "logger::level::disabled";
      case logger::level::emerg:    return "logger::level::emerg";
      case logger::level::alert:    return "logger::level::alert";
      case logger::level::crit:     return "logger::level::crit";
      case logger::level::err:      return "logger::level::err";
      case logger::level::warning:  return "logger::level::warning";
      case logger::level::notice:   return "logger::level::notice";
      case logger::level::info:     return "logger::level::info";
      case logger::level::debug:    return "logger::level::debug";
      default:                      return "<unknown logger::level>";
   }
}

namespace boost::redis::detail {

void read(multiplexer& mpx, std::string_view data)
{
   auto const ec = mpx.prepare_read();
   ignore_unused(ec);
   BOOST_ASSERT(ec == system::error_code{});
   auto const buffer = mpx.get_prepared_read_buffer();
   BOOST_ASSERT(buffer.size() >= data.size());
   std::copy(data.cbegin(), data.cend(), buffer.begin());
   mpx.commit_read(data.size());
}

// Operators to enable checking logs
bool operator==(const log_message& lhs, const log_message& rhs) noexcept
{
   return lhs.lvl == rhs.lvl && lhs.msg == rhs.msg;
}

std::ostream& operator<<(std::ostream& os, const log_message& v)
{
   return os << "log_message { .lvl=" << to_string(v.lvl) << ", .msg=" << v.msg << " }";
}

void log_fixture::check_log(std::initializer_list<const log_message> expected, source_location loc)
   const
{
   if (!BOOST_TEST_ALL_EQ(expected.begin(), expected.end(), msgs.begin(), msgs.end())) {
      std::cerr << "Called from " << loc << std::endl;
   }
}

logger log_fixture::make_logger()
{
   return logger(logger::level::debug, [&](logger::level lvl, std::string_view msg) {
      msgs.push_back({lvl, std::string(msg)});
   });
}

}  // namespace boost::redis::detail
