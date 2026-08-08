//  Copyright (C) 2026 Tim Blechmann
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/lockfree/mpsc_weak_queue.hpp>

#define BOOST_TEST_MAIN
#ifdef BOOST_LOCKFREE_INCLUDE_TESTS
#    include <boost/test/included/unit_test.hpp>
#else
#    include <boost/test/unit_test.hpp>
#endif

#include "test_common.hpp"

namespace {

using comprehensive_stress_tester_mpsc = comprehensive_stress_tester< 10, 1 >;

} // namespace

BOOST_AUTO_TEST_CASE( mpsc_weak_queue_comprehensive_stress_1_consumer )
{
    comprehensive_stress_tester_mpsc        tester;
    boost::lockfree::mpsc_weak_queue< int > q( 128 );
    tester.run( q );
}
