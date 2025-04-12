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

#define BOOST_RANDOM_URNG boost::random::xoshiro128f
#define BOOST_RANDOM_CPP11_URNG

// principal operation validated with CLHEP, values by experiment
#define BOOST_RANDOM_VALIDATION_VALUE 0.496670783f
#define BOOST_RANDOM_SEED_SEQ_VALIDATION_VALUE 0.380300581f

#include "test_generator.ipp"
