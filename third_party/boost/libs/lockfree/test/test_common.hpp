//  Copyright (C) 2011 Tim Blechmann
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

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
struct queue_stress_tester
{
    static const unsigned int buckets = 1 << 13;
#ifndef BOOST_LOCKFREE_STRESS_TEST
    static const long node_count = 5000;
#else
    static const long node_count = 5000000;
#endif
    const int reader_threads;
    const int writer_threads;

    std::atomic< int > writers_finished;

    static_hashed_set< long, buckets >      data;
    static_hashed_set< long, buckets >      dequeued;
    std::array< std::set< long >, buckets > returned;

    std::atomic< int > push_count, pop_count;

    queue_stress_tester( int reader, int writer ) :
        reader_threads( reader ),
        writer_threads( writer ),
        push_count( 0 ),
        pop_count( 0 )
    {}

    template < typename queue >
    void add_items( queue& stk )
    {
        for ( long i = 0; i != node_count; ++i ) {
            long id = generate_id< long >();

            bool inserted = data.insert( id );
            assert( inserted );
            (void)inserted;

            if ( Bounded )
                while ( stk.bounded_push( id ) == false ) {
#ifdef __VXWORKS__
                    std::this_thread::yield();
#endif
                }
            else
                while ( stk.push( id ) == false ) {
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
    void run( queue& stk )
    {
        BOOST_WARN( stk.is_lock_free() );
        writers_finished.store( 0 );

        boost::thread_group writer;
        boost::thread_group reader;

        BOOST_TEST_REQUIRE( stk.empty() );

        for ( int i = 0; i != reader_threads; ++i )
            reader.create_thread( [ & ] {
                get_items( stk );
            } );

        for ( int i = 0; i != writer_threads; ++i )
            writer.create_thread( [ & ] {
                add_items( stk );
            } );

        std::cout << "threads created" << std::endl;

        writer.join_all();

        std::cout << "writer threads joined, waiting for readers" << std::endl;

        reader.join_all();

        std::cout << "reader threads joined" << std::endl;

        BOOST_TEST_REQUIRE( data.count_nodes() == (size_t)0 );
        BOOST_TEST_REQUIRE( stk.empty() );

        BOOST_TEST_REQUIRE( push_count == pop_count );
        BOOST_TEST_REQUIRE( push_count == writer_threads * node_count );
    }
};

} // namespace impl

using impl::queue_stress_tester;

namespace impl {

template < bool Bounded = false >
struct stack_consume_all_atomic_stress_tester
{
    static const unsigned int buckets = 1 << 13;
#ifndef BOOST_LOCKFREE_STRESS_TEST
    static const long node_count = 5000;
#else
    static const long node_count = 5000000;
#endif
    const int reader_threads;
    const int writer_threads;

    std::atomic< int > writers_finished;

    static_hashed_set< long, buckets > data;
    static_hashed_set< long, buckets > dequeued;

    std::atomic< int > push_count, pop_count;

    stack_consume_all_atomic_stress_tester( int reader, int writer ) :
        reader_threads( reader ),
        writer_threads( writer ),
        push_count( 0 ),
        pop_count( 0 )
    {}

    template < typename stack_type >
    void add_items( stack_type& stk )
    {
        for ( long i = 0; i != node_count; ++i ) {
            long id = generate_id< long >();

            bool inserted = data.insert( id );
            assert( inserted );
            (void)inserted;

            if ( Bounded )
                while ( stk.bounded_push( id ) == false ) {
#ifdef __VXWORKS__
                    std::this_thread::yield();
#endif
                }
            else
                while ( stk.push( id ) == false ) {
#ifdef __VXWORKS__
                    std::this_thread::yield();
#endif
                }
            ++push_count;
        }
        writers_finished += 1;
    }

    template < typename stack_type >
    void get_items_atomic( stack_type& stk )
    {
        for ( ;; ) {
            size_t consumed = stk.consume_all_atomic( [ & ]( long id ) {
                bool erased   = data.erase( id );
                bool inserted = dequeued.insert( id );
                (void)erased;
                (void)inserted;
                assert( erased );
                assert( inserted );
                ++pop_count;
            } );

            if ( consumed == 0 && writers_finished.load() == writer_threads )
                break;

#ifdef __VXWORKS__
            std::this_thread::yield();
#endif
        }

        // drain remaining
        stk.consume_all_atomic( [ & ]( long id ) {
            bool erased   = data.erase( id );
            bool inserted = dequeued.insert( id );
            (void)erased;
            (void)inserted;
            assert( erased );
            assert( inserted );
            ++pop_count;
        } );
    }

    template < typename stack_type >
    void get_items_atomic_reversed( stack_type& stk )
    {
        for ( ;; ) {
            size_t consumed = stk.consume_all_atomic_reversed( [ & ]( long id ) {
                bool erased   = data.erase( id );
                bool inserted = dequeued.insert( id );
                (void)erased;
                (void)inserted;
                assert( erased );
                assert( inserted );
                ++pop_count;
            } );

            if ( consumed == 0 && writers_finished.load() == writer_threads )
                break;

#ifdef __VXWORKS__
            std::this_thread::yield();
#endif
        }

        // drain remaining
        stk.consume_all_atomic_reversed( [ & ]( long id ) {
            bool erased   = data.erase( id );
            bool inserted = dequeued.insert( id );
            (void)erased;
            (void)inserted;
            assert( erased );
            assert( inserted );
            ++pop_count;
        } );
    }

    template < typename stack_type, typename ReaderFunc >
    void run_impl( stack_type& stk, ReaderFunc reader_func )
    {
        BOOST_WARN( stk.is_lock_free() );
        writers_finished.store( 0 );

        boost::thread_group writer;
        boost::thread_group reader;

        BOOST_TEST_REQUIRE( stk.empty() );

        for ( int i = 0; i != reader_threads; ++i )
            reader.create_thread( [ &, reader_func ] {
                reader_func( stk );
            } );

        for ( int i = 0; i != writer_threads; ++i )
            writer.create_thread( [ & ] {
                add_items( stk );
            } );

        std::cout << "threads created" << std::endl;

        writer.join_all();

        std::cout << "writer threads joined, waiting for readers" << std::endl;

        reader.join_all();

        std::cout << "reader threads joined" << std::endl;

        BOOST_TEST_REQUIRE( data.count_nodes() == (size_t)0 );
        BOOST_TEST_REQUIRE( stk.empty() );

        BOOST_TEST_REQUIRE( push_count == pop_count );
        BOOST_TEST_REQUIRE( push_count == writer_threads * node_count );
    }

    template < typename stack_type >
    void run_atomic( stack_type& stk )
    {
        run_impl( stk, [ this ]( stack_type& s ) {
            get_items_atomic( s );
        } );
    }

    template < typename stack_type >
    void run_atomic_reversed( stack_type& stk )
    {
        run_impl( stk, [ this ]( stack_type& s ) {
            get_items_atomic_reversed( s );
        } );
    }
};

} // namespace impl

using impl::stack_consume_all_atomic_stress_tester;

namespace impl {

// Comprehensive stress tester with per-producer value ranges and per-consumer monotonicity tracking
// - 10 producer threads, each producing 900,000 sequential values
// - Producer i produces: 1000000*i, 1000000*i+1, ..., 1000000*i+899999
// - Multiple consumer threads, each tracking last-received value per producer
// - Validates strict monotonicity per producer and per-consumer consistency
template < int NumProducers = 10, int NumConsumers = 1 >
struct comprehensive_stress_tester
{
    enum
    {
        values_per_producer = 900000,
        base_multiplier     = 1000000,
    };

    std::atomic< int >  producers_finished { 0 };
    std::atomic< int >  validation_errors { 0 };
    std::atomic< long > total_values_consumed { 0 };

    // Per-consumer validation state
    struct consumer_state
    {
        std::array< int, NumProducers > last_received {};
    };
    std::array< consumer_state, NumConsumers > consumers;

    template < typename DataStructure >
    bool validate_value( int consumer_id, int value )
    {
        if ( consumer_id < 0 || consumer_id >= NumConsumers )
            return false;

        int producer_id = value / base_multiplier;
        int offset      = value % base_multiplier;

        // Check range
        if ( producer_id < 0 || producer_id >= NumProducers )
            return false;
        if ( offset < 0 || offset >= values_per_producer )
            return false;

        // Check strict monotonicity per producer for this consumer
        int& last = consumers[ consumer_id ].last_received[ producer_id ];
        if ( last != 0 && value <= last )
            return false;

        last = value;
        return true;
    }

    template < typename DataStructure >
    void produce_items( DataStructure& ds, int producer_id )
    {
        int base = base_multiplier * producer_id;
        for ( long i = 0; i < values_per_producer; ++i ) {
            int value = base + i;
            while ( !ds.push( value ) ) {
#ifdef __VXWORKS__
                std::this_thread::yield();
#endif
            }
        }
        producers_finished.fetch_add( 1 );
    }

    template < typename DataStructure >
    void consume_items( DataStructure& ds, int consumer_id )
    {
        while ( true ) {
            int value;
            if ( !ds.pop( value ) ) {
                if ( producers_finished.load() == NumProducers )
                    break;
#ifdef __VXWORKS__
                std::this_thread::yield();
#endif
                continue;
            }

            if ( !validate_value< DataStructure >( consumer_id, value ) )
                validation_errors.fetch_add( 1 );
            total_values_consumed.fetch_add( 1 );
        }

        // Drain remaining items
        int value;
        while ( ds.pop( value ) ) {
            if ( !validate_value< DataStructure >( consumer_id, value ) )
                validation_errors.fetch_add( 1 );
            total_values_consumed.fetch_add( 1 );
        }
    }

    template < typename DataStructure >
    void run( DataStructure& ds )
    {
        BOOST_WARN( ds.is_lock_free() );
        producers_finished.store( 0 );
        validation_errors.store( 0 );
        total_values_consumed.store( 0 );

        BOOST_TEST_REQUIRE( ds.empty() );

        boost::thread_group producers;
        boost::thread_group consumers_group;

        // Spawn producer threads
        for ( int i = 0; i < NumProducers; ++i )
            producers.create_thread( [ this, &ds, i ] {
                produce_items( ds, i );
            } );

        // Spawn consumer threads
        for ( int i = 0; i < NumConsumers; ++i )
            consumers_group.create_thread( [ this, &ds, i ] {
                consume_items( ds, i );
            } );

        std::cout << "comprehensive stress test: " << NumProducers << " producers, " << NumConsumers
                  << " consumers created" << std::endl;

        producers.join_all();
        std::cout << "producers finished" << std::endl;

        consumers_group.join_all();
        std::cout << "consumers finished" << std::endl;

        BOOST_TEST_REQUIRE( ds.empty() );
        BOOST_TEST_REQUIRE( validation_errors.load() == 0 );
        BOOST_TEST_REQUIRE( total_values_consumed.load() == NumProducers * values_per_producer );
    }
};

} // namespace impl

using impl::comprehensive_stress_tester;

namespace impl {

// Comprehensive stress tester for stacks - counts elements per producer range
// (stacks are LIFO; with concurrent multi-producer/multi-consumer, we can't enforce
// ordering, but we can validate that each element consumed belongs to a valid producer
// range and that the total count per producer matches across all consumers)
template < int NumProducers = 10, int NumConsumers = 1 >
struct comprehensive_stack_stress_tester
{
    enum
    {
        values_per_producer = 900000,
        base_multiplier     = 1000000,
    };

    std::atomic< int >  producers_finished { 0 };
    std::atomic< int >  validation_errors { 0 };
    std::atomic< long > total_values_consumed { 0 };

    // Per-consumer count of elements from each producer range
    struct consumer_state
    {
        std::array< long, NumProducers > producer_counts {};
    };
    std::array< consumer_state, NumConsumers > consumers;

    template < typename DataStructure >
    bool validate_value( int consumer_id, int value )
    {
        if ( consumer_id < 0 || consumer_id >= NumConsumers )
            return false;

        int producer_id = value / base_multiplier;
        int offset      = value % base_multiplier;

        // Check range
        if ( producer_id < 0 || producer_id >= NumProducers )
            return false;
        if ( offset < 0 || offset >= values_per_producer )
            return false;

        // Increment count for this producer
        consumers[ consumer_id ].producer_counts[ producer_id ]++;
        return true;
    }

    template < typename DataStructure >
    void produce_items( DataStructure& ds, int producer_id )
    {
        int base = base_multiplier * producer_id;
        for ( long i = 0; i < values_per_producer; ++i ) {
            int value = base + i;
            while ( !ds.push( value ) ) {
#ifdef __VXWORKS__
                std::this_thread::yield();
#endif
            }
        }
        producers_finished.fetch_add( 1 );
    }

    template < typename DataStructure >
    void consume_items( DataStructure& ds, int consumer_id )
    {
        while ( true ) {
            int value;
            if ( !ds.pop( value ) ) {
                if ( producers_finished.load() == NumProducers )
                    break;
#ifdef __VXWORKS__
                std::this_thread::yield();
#endif
                continue;
            }

            if ( !validate_value< DataStructure >( consumer_id, value ) )
                validation_errors.fetch_add( 1 );
            total_values_consumed.fetch_add( 1 );
        }

        // Drain remaining items
        int value;
        while ( ds.pop( value ) ) {
            if ( !validate_value< DataStructure >( consumer_id, value ) )
                validation_errors.fetch_add( 1 );
            total_values_consumed.fetch_add( 1 );
        }
    }

    template < typename DataStructure >
    void run( DataStructure& ds )
    {
        BOOST_WARN( ds.is_lock_free() );
        producers_finished.store( 0 );
        validation_errors.store( 0 );
        total_values_consumed.store( 0 );

        // Reset consumer counts
        for ( int i = 0; i < NumConsumers; ++i ) {
            for ( int j = 0; j < NumProducers; ++j ) {
                consumers[ i ].producer_counts[ j ] = 0;
            }
        }

        BOOST_TEST_REQUIRE( ds.empty() );

        boost::thread_group producers;
        boost::thread_group consumers_group;

        // Spawn producer threads
        for ( int i = 0; i < NumProducers; ++i )
            producers.create_thread( [ this, &ds, i ] {
                produce_items( ds, i );
            } );

        // Spawn consumer threads
        for ( int i = 0; i < NumConsumers; ++i )
            consumers_group.create_thread( [ this, &ds, i ] {
                consume_items( ds, i );
            } );

        std::cout << "comprehensive stack stress test: " << NumProducers << " producers, " << NumConsumers
                  << " consumers created" << std::endl;

        producers.join_all();
        std::cout << "producers finished" << std::endl;

        consumers_group.join_all();
        std::cout << "consumers finished" << std::endl;

        BOOST_TEST_REQUIRE( ds.empty() );
        BOOST_TEST_REQUIRE( validation_errors.load() == 0 );
        BOOST_TEST_REQUIRE( total_values_consumed.load() == NumProducers * values_per_producer );

        // Validate that sum of counts for each producer across all consumers equals values_per_producer
        for ( int producer_id = 0; producer_id < NumProducers; ++producer_id ) {
            long total_for_producer = 0;
            for ( int consumer_id = 0; consumer_id < NumConsumers; ++consumer_id ) {
                total_for_producer += consumers[ consumer_id ].producer_counts[ producer_id ];
            }
            BOOST_TEST_REQUIRE( total_for_producer == values_per_producer );
        }
    }
};

} // namespace impl

using impl::comprehensive_stack_stress_tester;
