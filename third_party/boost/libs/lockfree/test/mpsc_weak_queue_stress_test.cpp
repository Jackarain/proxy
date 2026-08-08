//  Copyright (C) 2024 Tim Blechmann
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
#include "test_helpers.hpp"

#include <array>
#include <atomic>
#include <cassert>
#include <iostream>
#ifdef __VXWORKS__
#    include <thread>
#endif

#include <boost/test/test_tools.hpp>
#include <boost/thread/thread.hpp>

namespace impl {

template < bool Bounded = false >
struct mpsc_weak_queue_stress_tester
{
    static const unsigned int buckets = 1 << 13;
#ifndef BOOST_LOCKFREE_STRESS_TEST
    static const long node_count = 5000;
#else
    static const long node_count = 5000000;
#endif
    const int writer_threads;

    std::atomic< int > writers_finished;

    static_hashed_set< long, buckets >      data;
    static_hashed_set< long, buckets >      dequeued;
    std::array< std::set< long >, buckets > returned;

    std::atomic< int > push_count, pop_count;

    explicit mpsc_weak_queue_stress_tester( int writer ) :
        writer_threads( writer ),
        push_count( 0 ),
        pop_count( 0 )
    {}

    template < typename queue >
    void add_items( queue& q )
    {
        for ( long i = 0; i != node_count; ++i ) {
            long id = generate_id< long >();

            bool inserted = data.insert( id );
            assert( inserted );
            (void)inserted;

            if ( Bounded )
                while ( q.bounded_push( id ) == false ) {
#ifdef __VXWORKS__
                    std::this_thread::yield();
#endif
                }
            else
                while ( q.push( id ) == false ) {
#ifdef __VXWORKS__
                    std::this_thread::yield();
#endif
                }
            ++push_count;
        }
        writers_finished += 1;
    }

    std::atomic< bool > running;

    template < typename queue >
    bool consume_element( queue& q )
    {
        long id;
        bool ret = q.pop( id );

        if ( !ret )
            return false;

        bool erased   = data.erase( id );
        bool inserted = dequeued.insert( id );
        (void)erased;
        (void)inserted;
        assert( erased );
        assert( inserted );
        ++pop_count;
        return true;
    }

    template < typename queue >
    void get_items( queue& q )
    {
        for ( ;; ) {
            bool received_element = consume_element( q );
            if ( received_element )
                continue;

            if ( writers_finished.load() == writer_threads )
                break;

#ifdef __VXWORKS__
            std::this_thread::yield();
#endif
        }

        while ( consume_element( q ) )
            ;
    }

    template < typename queue >
    void run( queue& q )
    {
        BOOST_WARN( q.is_lock_free() );
        writers_finished.store( 0 );

        boost::thread_group writer;
        boost::thread       reader;

        BOOST_TEST_REQUIRE( q.empty() );

        reader = boost::thread( [ & ] {
            get_items( q );
        } );

        for ( int i = 0; i != writer_threads; ++i )
            writer.create_thread( [ & ] {
                add_items( q );
            } );

        std::cout << "threads created" << std::endl;

        writer.join_all();

        std::cout << "writer threads joined, waiting for readers" << std::endl;

        reader.join();

        std::cout << "reader thread joined" << std::endl;

        BOOST_TEST_REQUIRE( data.count_nodes() == (size_t)0 );
        BOOST_TEST_REQUIRE( q.empty() );

        BOOST_TEST_REQUIRE( push_count == pop_count );
        BOOST_TEST_REQUIRE( push_count == writer_threads * node_count );
    }
};

} // namespace impl

using impl::mpsc_weak_queue_stress_tester;


BOOST_AUTO_TEST_CASE( mpsc_weak_queue_test_unbounded )
{
    typedef mpsc_weak_queue_stress_tester< false > tester_type;
    std::unique_ptr< tester_type >                 tester( new tester_type( 4 ) );

    boost::lockfree::mpsc_weak_queue< long > q( 128 );
    tester->run( q );
}

BOOST_AUTO_TEST_CASE( mpsc_weak_queue_test_unbounded_single_writer )
{
    typedef mpsc_weak_queue_stress_tester< false > tester_type;
    std::unique_ptr< tester_type >                 tester( new tester_type( 1 ) );

    boost::lockfree::mpsc_weak_queue< long > q( 128 );
    tester->run( q );
}

BOOST_AUTO_TEST_CASE( mpsc_weak_queue_test_bounded )
{
    typedef mpsc_weak_queue_stress_tester< true > tester_type;
    std::unique_ptr< tester_type >                tester( new tester_type( 4 ) );

    boost::lockfree::mpsc_weak_queue< long > q( 128 );
    tester->run( q );
}
