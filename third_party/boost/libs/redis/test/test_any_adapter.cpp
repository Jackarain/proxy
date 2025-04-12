/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/ignore.hpp>
#include <boost/redis/response.hpp>
#include <boost/redis/adapter/any_adapter.hpp>
#include <string>
#define BOOST_TEST_MODULE any_adapter
#include <boost/test/included/unit_test.hpp>

using boost::redis::generic_response;
using boost::redis::response;
using boost::redis::ignore;
using boost::redis::any_adapter;

BOOST_AUTO_TEST_CASE(any_adapter_response_types)
{
   // any_adapter can be used with any supported responses
   response<int> r1;
   response<int, std::string> r2;
   generic_response r3;

   BOOST_CHECK_NO_THROW(any_adapter{r1});
   BOOST_CHECK_NO_THROW(any_adapter{r2});
   BOOST_CHECK_NO_THROW(any_adapter{r3});
   BOOST_CHECK_NO_THROW(any_adapter{ignore});
}

BOOST_AUTO_TEST_CASE(any_adapter_copy_move)
{
   // any_adapter can be copied/moved
   response<int, std::string> r;
   any_adapter ad1 {r};

   // copy constructor
   any_adapter ad2 {ad1};
   
   // move constructor
   any_adapter ad3 {std::move(ad2)};

   // copy assignment
   BOOST_CHECK_NO_THROW(ad2 = ad1);

   // move assignment
   BOOST_CHECK_NO_THROW(ad2 = std::move(ad1)); 
}
