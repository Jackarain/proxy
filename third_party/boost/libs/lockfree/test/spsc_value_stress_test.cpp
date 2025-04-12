//  Copyright (C) 2011-2024 Tim Blechmann
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/lockfree/spsc_value.hpp>

#define BOOST_TEST_MAIN
#ifdef BOOST_LOCKFREE_INCLUDE_TESTS
#    include <boost/test/included/unit_test.hpp>
#else
#    include <boost/test/unit_test.hpp>
#endif

#include <boost/optional.hpp>

#include <atomic>
#include <thread>

// #define BOOST_LOCKFREE_STRESS_TEST

#ifndef BOOST_LOCKFREE_STRESS_TEST
static const uint64_t nodes_per_thread = 100000;
#else
static const uint64_t nodes_per_thread = 100000000;
#endif


BOOST_AUTO_TEST_CASE( spsc_value_stress_test )
{
    boost::lockfree::spsc_value< uint64_t > v;

    std::atomic< bool > done;

    std::thread producer( [ & ] {
        for ( uint64_t i = 0; i != nodes_per_thread; ++i )
            v.write( i );
        done = true;
    } );

    boost::optional< uint64_t > consumed;
    while ( !done.load( std::memory_order_relaxed ) ) {
        uint64_t out;
        bool     read_success = v.read( out );

        if ( !read_success ) {
            std::this_thread::yield();
            continue;
        }

        if ( consumed )
            BOOST_TEST_REQUIRE( out > *consumed );
        consumed = out;
    }

    producer.join();
}

BOOST_AUTO_TEST_CASE( spsc_value_stress_test_allow_multiple_reads )
{
    boost::lockfree::spsc_value< uint64_t, boost::lockfree::allow_multiple_reads< true > > v;

    std::atomic< bool > done;

    std::thread producer( [ & ] {
        for ( uint64_t i = 0; i != nodes_per_thread; ++i ) {
            std::this_thread::yield();
            v.write( i );
        }
        done = true;
    } );

    boost::optional< uint64_t > consumed;
    while ( !done.load( std::memory_order_relaxed ) ) {
        uint64_t out {};
        bool     read_success = v.read( out );

        if ( !read_success ) {
            std::this_thread::yield();
            continue;
        }

        if ( consumed )
            BOOST_TEST_REQUIRE( out >= *consumed );
        consumed = out;
    }

    producer.join();
}
