//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/config.hpp>
#include <boost/redis/impl/sentinel_utils.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/system/error_code.hpp>

#include <vector>

using namespace boost::redis;
using detail::update_sentinel_list;
using boost::system::error_code;

// Operators
namespace boost::redis {

std::ostream& operator<<(std::ostream& os, const address& addr)
{
   return os << "address{ .host=" << addr.host << ", .port=" << addr.port << " }";
}

}  // namespace boost::redis

namespace {

// The only Sentinel resolved the address successfully, and there's no newly discovered Sentinels
void test_single_sentinel()
{
   const std::vector<address> initial_sentinels{
      {"host1", "1000"}
   };
   std::vector<address> sentinels{initial_sentinels};

   update_sentinel_list(sentinels, 0u, {}, initial_sentinels);

   BOOST_TEST_ALL_EQ(
      sentinels.begin(),
      sentinels.end(),
      initial_sentinels.begin(),
      initial_sentinels.end());
}

// Some new Sentinels were discovered using SENTINEL SENTINELS
void test_new_sentinels()
{
   const std::vector<address> initial_sentinels{
      {"host1", "1000"}
   };
   std::vector<address> sentinels{initial_sentinels};
   const address new_sentinels[]{
      {"host2", "2000"},
      {"host3", "3000"},
   };

   update_sentinel_list(sentinels, 0u, new_sentinels, initial_sentinels);

   const address expected_sentinels[]{
      {"host1", "1000"},
      {"host2", "2000"},
      {"host3", "3000"},
   };

   BOOST_TEST_ALL_EQ(
      sentinels.begin(),
      sentinels.end(),
      std::begin(expected_sentinels),
      std::end(expected_sentinels));
}

// Some of the new Sentinels are already in the list
void test_new_sentinels_known()
{
   const std::vector<address> initial_sentinels{
      {"host1", "1000"},
      {"host2", "2000"},
   };
   std::vector<address> sentinels{initial_sentinels};
   const address new_sentinels[]{
      {"host2", "2000"},
      {"host3", "3000"},
   };

   update_sentinel_list(sentinels, 0u, new_sentinels, initial_sentinels);

   const address expected_sentinels[]{
      {"host1", "1000"},
      {"host2", "2000"},
      {"host3", "3000"},
   };

   BOOST_TEST_ALL_EQ(
      sentinels.begin(),
      sentinels.end(),
      std::begin(expected_sentinels),
      std::end(expected_sentinels));
}

// The Sentinel that succeeded should be placed first
void test_success_sentinel_not_first()
{
   const std::vector<address> initial_sentinels{
      {"host1", "1000"},
      {"host2", "2000"},
      {"host3", "3000"},
   };
   std::vector<address> sentinels{initial_sentinels};
   const address new_sentinels[]{
      {"host1", "1000"},
      {"host2", "2000"},
   };

   update_sentinel_list(sentinels, 2u, new_sentinels, initial_sentinels);

   const address expected_sentinels[]{
      {"host3", "3000"},
      {"host1", "1000"},
      {"host2", "2000"},
   };

   BOOST_TEST_ALL_EQ(
      sentinels.begin(),
      sentinels.end(),
      std::begin(expected_sentinels),
      std::end(expected_sentinels));
}

// If a discovered Sentinel is not returned in subsequent iterations, it's removed from the list
void test_new_sentinel_removed()
{
   const std::vector<address> initial_sentinels{
      {"host1", "1000"},
   };
   std::vector<address> sentinels{
      {"host1", "1000"},
      {"host4", "4000"},
   };
   const address new_sentinels[]{
      {"host2", "2000"},
      {"host3", "3000"},
   };

   update_sentinel_list(sentinels, 0u, new_sentinels, initial_sentinels);

   const address expected_sentinels[]{
      {"host1", "1000"},
      {"host2", "2000"},
      {"host3", "3000"},
   };

   BOOST_TEST_ALL_EQ(
      sentinels.begin(),
      sentinels.end(),
      std::begin(expected_sentinels),
      std::end(expected_sentinels));
}

// Bootstrap Sentinels are never removed
void test_bootstrap_sentinel_removed()
{
   const std::vector<address> initial_sentinels{
      {"host1", "1000"},
      {"host2", "2000"},
      {"host3", "3000"},
   };
   std::vector<address> sentinels{
      {"host1", "1000"},
      {"host2", "2000"},
      {"host3", "3000"},
      {"host4", "4000"},
      {"host5", "5000"},
   };
   const address new_sentinels[]{
      {"host2", "2000"},
      {"host4", "4000"},
   };

   update_sentinel_list(sentinels, 0u, new_sentinels, initial_sentinels);

   const address expected_sentinels[]{
      {"host1", "1000"},
      {"host2", "2000"},
      {"host4", "4000"},
      {"host3", "3000"}, // bootstrap Sentinels placed last
   };

   BOOST_TEST_ALL_EQ(
      sentinels.begin(),
      sentinels.end(),
      std::begin(expected_sentinels),
      std::end(expected_sentinels));
}

}  // namespace

int main()
{
   test_single_sentinel();
   test_new_sentinels();
   test_new_sentinels_known();
   test_success_sentinel_not_first();
   test_new_sentinel_removed();
   test_bootstrap_sentinel_removed();

   return boost::report_errors();
}
