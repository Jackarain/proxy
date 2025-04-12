/* test_xoshiro256d.cpp
 *
 * Copyright Steven Watanabe 2011
 * Copyright Matt Borland 2022
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * $Id$
 *
 */

#include <boost/random/xoshiro.hpp>
#include <cstdint>
#include <cmath>

#define BOOST_RANDOM_URNG boost::random::xoshiro512d
#define BOOST_RANDOM_CPP11_URNG

// principal operation validated with CLHEP, values by experiment
#define BOOST_RANDOM_VALIDATION_VALUE 0.85594919700533156
#define BOOST_RANDOM_SEED_SEQ_VALIDATION_VALUE 0.25120475433393952

// Since we are using splitmix64 we need to allow 64 bit seeds
// The test harness only allows for 32 bit seeds
#define BOOST_RANDOM_PROVIDED_SEED_TYPE std::uint64_t

#include "test_generator.ipp"
