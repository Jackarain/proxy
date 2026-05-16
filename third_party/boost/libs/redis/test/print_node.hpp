
/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_TEST_PRINT_NODE_HPP
#define BOOST_REDIS_TEST_PRINT_NODE_HPP

#include <boost/redis/resp3/node.hpp>
#include <boost/redis/resp3/type.hpp>

#include <ostream>

namespace boost::redis::resp3 {

template <class String>
std::ostream& operator<<(std::ostream& os, basic_node<String> const& nd)
{
   return os << "node{ .data_type=" << to_string(nd.data_type)
             << ", .aggregate_size=" << nd.aggregate_size << ", .depth=" << nd.depth
             << ", .value=" << nd.value << "}";
}

}  // namespace boost::redis::resp3

#endif  // BOOST_REDIS_TEST_SANSIO_UTILS_HPP
