//  Copyright (C) 2026 Tim Blechmann
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/lockfree/bounded_ticket_queue.hpp>

#define BOOST_TEST_MAIN
#ifdef BOOST_LOCKFREE_INCLUDE_TESTS
#    include <boost/test/included/unit_test.hpp>
#else
#    include <boost/test/unit_test.hpp>
#endif

#include "test_common.hpp"

using namespace boost::lockfree;

BOOST_AUTO_TEST_CASE( bounded_ticket_queue_comprehensive_mpmc_1c )
{
    comprehensive_stress_tester< 10, 1 > tester;
    bounded_ticket_queue< int >          q( 8192 );
    tester.run( q );
}

BOOST_AUTO_TEST_CASE( bounded_ticket_queue_comprehensive_mpmc_4c )
{
    comprehensive_stress_tester< 10, 4 > tester;
    bounded_ticket_queue< int >          q( 8192 );
    tester.run( q );
}

BOOST_AUTO_TEST_CASE( bounded_ticket_queue_comprehensive_mpsc )
{
    comprehensive_stress_tester< 10, 1 >                 tester;
    bounded_ticket_queue< int, single_consumer< true > > q( 8192 );
    tester.run( q );
}

BOOST_AUTO_TEST_CASE( bounded_ticket_queue_comprehensive_spmc )
{
    comprehensive_stress_tester< 1, 4 >                  tester;
    bounded_ticket_queue< int, single_producer< true > > q( 8192 );
    tester.run( q );
}

BOOST_AUTO_TEST_CASE( bounded_ticket_queue_comprehensive_spsc )
{
    comprehensive_stress_tester< 1, 1 >                                           tester;
    bounded_ticket_queue< int, single_producer< true >, single_consumer< true > > q( 8192 );
    tester.run( q );
}
