/* Copyright 2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See https://www.boost.org/libs/bloom for library home page.
 */

#ifndef BOOST_BLOOM_TEST_TEST_TYPES_HPP
#define BOOST_BLOOM_TEST_TEST_TYPES_HPP

#include <boost/bloom/block.hpp>
#include <boost/bloom/fast_multiblock32.hpp>
#include <boost/bloom/fast_multiblock64.hpp>
#include <boost/bloom/filter.hpp>
#include <boost/bloom/multiblock.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>
#include <boost/mp11/utility.hpp>
#include <cstdint>
#include <string>

using test_types=boost::mp11::mp_list<
  boost::bloom::filter<
    int,2
  >,
  boost::bloom::filter<
   std::string,1,boost::bloom::block<std::uint16_t,3>,1
  >,
  boost::bloom::filter<
   int,1,boost::bloom::block<std::uint32_t[4],4>
  >,
  boost::bloom::filter<
    std::size_t,1,boost::bloom::multiblock<std::uint64_t,3>
  >,
  boost::bloom::filter<
    std::size_t,1,boost::bloom::multiblock<unsigned char[4],3>,1
  >,
  boost::bloom::filter<
    unsigned char,1,boost::bloom::fast_multiblock32<5>,2
  >,
  boost::bloom::filter<
    int,1,boost::bloom::fast_multiblock64<11>
  >
>;

using identity_test_types=
  boost::mp11::mp_transform<boost::mp11::mp_identity,test_types>;

#endif
