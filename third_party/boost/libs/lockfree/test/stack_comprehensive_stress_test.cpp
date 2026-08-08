//  Copyright (C) 2026 Tim Blechmann
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/lockfree/stack.hpp>

#define BOOST_TEST_MAIN
#ifdef BOOST_LOCKFREE_INCLUDE_TESTS
#    include <boost/test/included/unit_test.hpp>
#else
#    include <boost/test/unit_test.hpp>
#endif

#include "test_common.hpp"

namespace {

using comprehensive_stack_stress_tester_1c = comprehensive_stack_stress_tester< 10, 1 >;
using comprehensive_stack_stress_tester_4c = comprehensive_stack_stress_tester< 10, 4 >;

} // namespace

BOOST_AUTO_TEST_CASE( stack_comprehensive_stress_unbounded_1_consumer )
{
    comprehensive_stack_stress_tester_1c tester;
    boost::lockfree::stack< int >        s( 128 );
    tester.run( s );
}

BOOST_AUTO_TEST_CASE( stack_comprehensive_stress_unbounded_4_consumers )
{
    comprehensive_stack_stress_tester_4c tester;
    boost::lockfree::stack< int >        s( 128 );
    tester.run( s );
}
