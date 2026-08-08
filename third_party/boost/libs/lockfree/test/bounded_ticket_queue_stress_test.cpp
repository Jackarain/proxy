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

#include <atomic>
#include <cassert>
#include <iostream>
#include <memory>
#ifdef __VXWORKS__
#    include <thread>
#endif

#include <boost/test/test_tools.hpp>
#include <boost/thread/thread.hpp>

namespace impl {

struct bounded_ticket_queue_stress_tester
{
    static const unsigned int buckets = 1 << 13;
#ifndef BOOST_LOCKFREE_STRESS_TEST
    static const long node_count = 5000;
#else
    static const long node_count = 5000000;
#endif
    const int writer_threads;
    const int reader_threads;

    std::atomic< int > writers_finished;

    static_hashed_set< long, buckets > data;
    static_hashed_set< long, buckets > dequeued;

    std::atomic< int > push_count, pop_count;

    explicit bounded_ticket_queue_stress_tester( int writers, int readers ) :
        writer_threads( writers ),
        reader_threads( readers ),
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

            while ( q.push( id ) == false )
                ;

            ++push_count;
        }
        writers_finished += 1;
    }

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
        boost::thread_group reader;

        BOOST_TEST_REQUIRE( q.empty() );

        for ( int i = 0; i != reader_threads; ++i )
            reader.create_thread( [ & ] {
                get_items( q );
            } );

        for ( int i = 0; i != writer_threads; ++i )
            writer.create_thread( [ & ] {
                add_items( q );
            } );

        std::cout << "threads created" << std::endl;

        writer.join_all();

        std::cout << "writer threads joined, waiting for readers" << std::endl;

        reader.join_all();

        std::cout << "reader threads joined" << std::endl;

        BOOST_TEST_REQUIRE( data.count_nodes() == (size_t)0 );
        BOOST_TEST_REQUIRE( q.empty() );

        BOOST_TEST_REQUIRE( push_count == pop_count );
        BOOST_TEST_REQUIRE( push_count == writer_threads * node_count );
    }
};

} // namespace impl

using impl::bounded_ticket_queue_stress_tester;


BOOST_AUTO_TEST_CASE( bounded_ticket_queue_four_writers_one_reader )
{
    auto tester = std::make_unique< bounded_ticket_queue_stress_tester >( 4, 1 );

    boost::lockfree::bounded_ticket_queue< long > q( 4096 );
    tester->run( q );
}

BOOST_AUTO_TEST_CASE( bounded_ticket_queue_four_writers_four_readers )
{
    auto tester = std::make_unique< bounded_ticket_queue_stress_tester >( 4, 4 );

    boost::lockfree::bounded_ticket_queue< long > q( 4096 );
    tester->run( q );
}

BOOST_AUTO_TEST_CASE( bounded_ticket_queue_one_writer_four_readers )
{
    auto tester = std::make_unique< bounded_ticket_queue_stress_tester >( 1, 4 );

    boost::lockfree::bounded_ticket_queue< long > q( 4096 );
    tester->run( q );
}

BOOST_AUTO_TEST_CASE( bounded_ticket_queue_one_writer_one_reader_mpsc )
{
    auto tester = std::make_unique< bounded_ticket_queue_stress_tester >( 4, 1 );

    boost::lockfree::bounded_ticket_queue< long, boost::lockfree::single_consumer< true > > q( 4096 );
    tester->run( q );
}

BOOST_AUTO_TEST_CASE( bounded_ticket_queue_one_writer_one_reader_spmc )
{
    auto tester = std::make_unique< bounded_ticket_queue_stress_tester >( 1, 1 );

    boost::lockfree::bounded_ticket_queue< long, boost::lockfree::single_producer< true > > q( 4096 );
    tester->run( q );
}

BOOST_AUTO_TEST_CASE( bounded_ticket_queue_one_writer_one_reader_spsc )
{
    auto tester = std::make_unique< bounded_ticket_queue_stress_tester >( 1, 1 );

    boost::lockfree::
        bounded_ticket_queue< long, boost::lockfree::single_producer< true >, boost::lockfree::single_consumer< true > >
            q( 4096 );
    tester->run( q );
}
