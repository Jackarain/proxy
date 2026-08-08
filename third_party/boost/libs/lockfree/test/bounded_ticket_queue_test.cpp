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

#include <memory>

using namespace boost::lockfree;

// ---- MPMC (multi-producer, multi-consumer, both false) -----

BOOST_AUTO_TEST_CASE( mpmc_simple_test )
{
    bounded_ticket_queue< int > f( 64 );

    BOOST_TEST_WARN( f.is_lock_free() );

    BOOST_TEST_REQUIRE( f.empty() );
    f.push( 1 );
    f.push( 2 );

    int i1( 0 ), i2( 0 );

    BOOST_TEST_REQUIRE( f.pop( i1 ) );
    BOOST_TEST_REQUIRE( i1 == 1 );

    BOOST_TEST_REQUIRE( f.pop( i2 ) );
    BOOST_TEST_REQUIRE( i2 == 2 );
    BOOST_TEST_REQUIRE( f.empty() );
}

BOOST_AUTO_TEST_CASE( mpmc_simple_test_capacity )
{
    bounded_ticket_queue< int, capacity< 64 > > f;

    BOOST_TEST_WARN( f.is_lock_free() );

    BOOST_TEST_REQUIRE( f.empty() );
    f.push( 1 );
    f.push( 2 );

    int i1( 0 ), i2( 0 );

    BOOST_TEST_REQUIRE( f.pop( i1 ) );
    BOOST_TEST_REQUIRE( i1 == 1 );

    BOOST_TEST_REQUIRE( f.pop( i2 ) );
    BOOST_TEST_REQUIRE( i2 == 2 );
    BOOST_TEST_REQUIRE( f.empty() );
}

BOOST_AUTO_TEST_CASE( mpmc_exhausted )
{
    bounded_ticket_queue< int > f( 4 );

    BOOST_TEST_REQUIRE( f.push( 1 ) );
    BOOST_TEST_REQUIRE( f.push( 2 ) );
    BOOST_TEST_REQUIRE( f.push( 3 ) );
    BOOST_TEST_REQUIRE( f.push( 4 ) );
    BOOST_TEST_REQUIRE( !f.push( 5 ) );

    int out;
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 1 );
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 2 );
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 3 );
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 4 );
    BOOST_TEST_REQUIRE( !f.pop( out ) );
    BOOST_TEST_REQUIRE( f.empty() );
}

BOOST_AUTO_TEST_CASE( mpmc_exhausted_capacity )
{
    bounded_ticket_queue< int, capacity< 4 > > f;

    BOOST_TEST_REQUIRE( f.push( 1 ) );
    BOOST_TEST_REQUIRE( f.push( 2 ) );
    BOOST_TEST_REQUIRE( f.push( 3 ) );
    BOOST_TEST_REQUIRE( f.push( 4 ) );
    BOOST_TEST_REQUIRE( !f.push( 5 ) );

    int out;
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 1 );
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 2 );
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 3 );
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 4 );
    BOOST_TEST_REQUIRE( !f.pop( out ) );
    BOOST_TEST_REQUIRE( f.empty() );
}

BOOST_AUTO_TEST_CASE( mpmc_consume_one_test )
{
    bounded_ticket_queue< int > f( 64 );

    BOOST_TEST_REQUIRE( f.empty() );

    f.push( 1 );
    f.push( 2 );

    bool success1 = f.consume_one( []( int i ) {
        BOOST_TEST_REQUIRE( i == 1 );
    } );

    bool success2 = f.consume_one( []( int i ) {
        BOOST_TEST_REQUIRE( i == 2 );
    } );

    BOOST_TEST_REQUIRE( success1 );
    BOOST_TEST_REQUIRE( success2 );

    BOOST_TEST_REQUIRE( f.empty() );
}

BOOST_AUTO_TEST_CASE( mpmc_consume_all_test )
{
    bounded_ticket_queue< int > f( 64 );

    BOOST_TEST_REQUIRE( f.empty() );

    f.push( 1 );
    f.push( 2 );

    size_t consumed = f.consume_all( []( int i ) {} );

    BOOST_TEST_REQUIRE( consumed == 2u );

    BOOST_TEST_REQUIRE( f.empty() );
}

BOOST_AUTO_TEST_CASE( mpmc_convert_pop_test )
{
    bounded_ticket_queue< int* > f( 128 );
    BOOST_TEST_REQUIRE( f.empty() );
    f.push( new int( 1 ) );
    f.push( new int( 2 ) );
    f.push( new int( 3 ) );
    f.push( new int( 4 ) );

    {
        int* i1;

        BOOST_TEST_REQUIRE( f.pop( i1 ) );
        BOOST_TEST_REQUIRE( *i1 == 1 );
        delete i1;
    }

    {
        boost::shared_ptr< int > i2;
        BOOST_TEST_REQUIRE( f.pop( i2 ) );
        BOOST_TEST_REQUIRE( *i2 == 2 );
    }

    {
        std::unique_ptr< int > i3;
        BOOST_TEST_REQUIRE( f.pop( i3 ) );

        BOOST_TEST_REQUIRE( *i3 == 3 );
    }

    {
        std::shared_ptr< int > i4;
        BOOST_TEST_REQUIRE( f.pop( i4 ) );

        BOOST_TEST_REQUIRE( *i4 == 4 );
    }

    BOOST_TEST_REQUIRE( f.empty() );
}

BOOST_AUTO_TEST_CASE( mpmc_with_allocator )
{
    using allocator_type = std::allocator< char >;

    using queue_t = bounded_ticket_queue< char, allocator< allocator_type > >;

    auto allocator = queue_t::allocator {};

    {
        queue_t q_with_size_and_allocator {
            16,
            allocator,
        };
    }
    {
        queue_t q_with_size_and_allocator {
            16,
            allocator_type {},
        };
    }
}

BOOST_AUTO_TEST_CASE( mpmc_move_semantics )
{
    bounded_ticket_queue< int, capacity< 128 > > q;

    q.push( 0 );
    q.push( 1 );

    auto two = 2;
    q.push( std::move( two ) );

    int out;
    BOOST_TEST_REQUIRE( q.pop( out ) );
    BOOST_TEST_REQUIRE( out == 0 );

    q.consume_one( []( int one ) {
        BOOST_TEST_REQUIRE( one == 1 );
    } );

    q.consume_all( []( int ) {} );
}

#if !defined( BOOST_NO_CXX17_HDR_OPTIONAL )

BOOST_AUTO_TEST_CASE( mpmc_uses_optional )
{
    bounded_ticket_queue< int > stk( 8 );

    bool pop_to_nullopt = stk.pop( uses_optional ) == std::nullopt;
    BOOST_TEST_REQUIRE( pop_to_nullopt );

    stk.push( 53 );
    bool pop_to_optional = stk.pop( uses_optional ) == 53;
    BOOST_TEST_REQUIRE( pop_to_optional );
}

BOOST_AUTO_TEST_CASE( mpmc_uses_optional_capacity )
{
    bounded_ticket_queue< int, capacity< 64 > > q;

    bool pop_to_nullopt = q.pop( uses_optional ) == std::nullopt;
    BOOST_TEST_REQUIRE( pop_to_nullopt );

    q.push( 53 );
    bool pop_to_optional = q.pop( uses_optional ) == 53;
    BOOST_TEST_REQUIRE( pop_to_optional );
}

#endif

BOOST_AUTO_TEST_CASE( mpmc_empty_pop_test )
{
    bounded_ticket_queue< int > f( 64 );

    int out = 0xDEAD;
    BOOST_TEST_REQUIRE( !f.pop( out ) );
    BOOST_TEST_REQUIRE( !f.consume_one( []( int ) {} ) );
    BOOST_TEST_REQUIRE( f.consume_all( []( int ) {} ) == 0u );
}

BOOST_AUTO_TEST_CASE( mpmc_push_pop_many )
{
    bounded_ticket_queue< int > f( 128 );

    for ( int i = 0; i < 100; ++i )
        BOOST_TEST_REQUIRE( f.push( i ) );

    for ( int i = 0; i < 100; ++i ) {
        int out;
        BOOST_TEST_REQUIRE( f.pop( out ) );
        BOOST_TEST_REQUIRE( out == i );
    }
    BOOST_TEST_REQUIRE( f.empty() );
}

BOOST_AUTO_TEST_CASE( mpmc_push_pop_many_capacity )
{
    bounded_ticket_queue< int, capacity< 128 > > f;

    for ( int i = 0; i < 100; ++i )
        BOOST_TEST_REQUIRE( f.push( i ) );

    for ( int i = 0; i < 100; ++i ) {
        int out;
        BOOST_TEST_REQUIRE( f.pop( out ) );
        BOOST_TEST_REQUIRE( out == i );
    }
    BOOST_TEST_REQUIRE( f.empty() );
}

BOOST_AUTO_TEST_CASE( mpmc_move_only_types )
{
    bounded_ticket_queue< std::unique_ptr< int >, capacity< 128 > > q;

    q.push( std::make_unique< int >( 0 ) );
    q.push( std::make_unique< int >( 1 ) );

    auto two = std::make_unique< int >( 2 );
    q.push( std::move( two ) );

    std::unique_ptr< int > out;
    BOOST_TEST_REQUIRE( q.pop( out ) );
    BOOST_TEST_REQUIRE( *out == 0 );

    q.consume_one( []( std::unique_ptr< int > one ) {
        BOOST_TEST_REQUIRE( *one == 1 );
    } );

    q.consume_all( []( std::unique_ptr< int > ) {} );
}

BOOST_AUTO_TEST_CASE( mpmc_wrap_around )
{
    bounded_ticket_queue< int > f( 4 );

    BOOST_TEST_REQUIRE( f.push( 10 ) );
    BOOST_TEST_REQUIRE( f.push( 20 ) );
    BOOST_TEST_REQUIRE( f.push( 30 ) );
    BOOST_TEST_REQUIRE( f.push( 40 ) );
    BOOST_TEST_REQUIRE( !f.push( 50 ) );

    int out;
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 10 );
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 20 );
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 30 );
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 40 );
    BOOST_TEST_REQUIRE( !f.pop( out ) );
    BOOST_TEST_REQUIRE( f.empty() );

    BOOST_TEST_REQUIRE( f.push( 50 ) );
    BOOST_TEST_REQUIRE( f.push( 60 ) );
    BOOST_TEST_REQUIRE( f.push( 70 ) );
    BOOST_TEST_REQUIRE( f.push( 80 ) );
    BOOST_TEST_REQUIRE( !f.push( 90 ) );

    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 50 );
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 60 );
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 70 );
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 80 );
    BOOST_TEST_REQUIRE( !f.pop( out ) );
    BOOST_TEST_REQUIRE( f.empty() );
}

// ---- MPSC (single_consumer<true>) ----

BOOST_AUTO_TEST_CASE( mpsc_simple_test )
{
    bounded_ticket_queue< int, single_consumer< true > > f( 64 );

    BOOST_TEST_WARN( f.is_lock_free() );

    BOOST_TEST_REQUIRE( f.empty() );
    f.push( 1 );
    f.push( 2 );

    int i1( 0 ), i2( 0 );

    BOOST_TEST_REQUIRE( f.pop( i1 ) );
    BOOST_TEST_REQUIRE( i1 == 1 );

    BOOST_TEST_REQUIRE( f.pop( i2 ) );
    BOOST_TEST_REQUIRE( i2 == 2 );
    BOOST_TEST_REQUIRE( f.empty() );
}

BOOST_AUTO_TEST_CASE( mpsc_simple_test_capacity )
{
    bounded_ticket_queue< int, capacity< 64 >, single_consumer< true > > f;

    BOOST_TEST_WARN( f.is_lock_free() );

    BOOST_TEST_REQUIRE( f.empty() );
    f.push( 1 );
    f.push( 2 );

    int i1( 0 ), i2( 0 );

    BOOST_TEST_REQUIRE( f.pop( i1 ) );
    BOOST_TEST_REQUIRE( i1 == 1 );

    BOOST_TEST_REQUIRE( f.pop( i2 ) );
    BOOST_TEST_REQUIRE( i2 == 2 );
    BOOST_TEST_REQUIRE( f.empty() );
}

BOOST_AUTO_TEST_CASE( mpsc_exhausted )
{
    bounded_ticket_queue< int, single_consumer< true > > f( 4 );

    BOOST_TEST_REQUIRE( f.push( 1 ) );
    BOOST_TEST_REQUIRE( f.push( 2 ) );
    BOOST_TEST_REQUIRE( f.push( 3 ) );
    BOOST_TEST_REQUIRE( f.push( 4 ) );
    BOOST_TEST_REQUIRE( !f.push( 5 ) );

    int out;
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 1 );
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 2 );
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 3 );
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 4 );
    BOOST_TEST_REQUIRE( !f.pop( out ) );
    BOOST_TEST_REQUIRE( f.empty() );
}

BOOST_AUTO_TEST_CASE( mpsc_consume_one_test )
{
    bounded_ticket_queue< int, single_consumer< true > > f( 64 );

    BOOST_TEST_REQUIRE( f.empty() );

    f.push( 1 );
    f.push( 2 );

    bool success1 = f.consume_one( []( int i ) {
        BOOST_TEST_REQUIRE( i == 1 );
    } );

    bool success2 = f.consume_one( []( int i ) {
        BOOST_TEST_REQUIRE( i == 2 );
    } );

    BOOST_TEST_REQUIRE( success1 );
    BOOST_TEST_REQUIRE( success2 );

    BOOST_TEST_REQUIRE( f.empty() );
}

BOOST_AUTO_TEST_CASE( mpsc_consume_all_test )
{
    bounded_ticket_queue< int, single_consumer< true > > f( 64 );

    BOOST_TEST_REQUIRE( f.empty() );

    f.push( 1 );
    f.push( 2 );

    size_t consumed = f.consume_all( []( int i ) {} );

    BOOST_TEST_REQUIRE( consumed == 2u );

    BOOST_TEST_REQUIRE( f.empty() );
}

BOOST_AUTO_TEST_CASE( mpsc_convert_pop_test )
{
    bounded_ticket_queue< int*, single_consumer< true > > f( 128 );
    BOOST_TEST_REQUIRE( f.empty() );
    f.push( new int( 1 ) );
    f.push( new int( 2 ) );
    f.push( new int( 3 ) );
    f.push( new int( 4 ) );

    {
        int* i1;
        BOOST_TEST_REQUIRE( f.pop( i1 ) );
        BOOST_TEST_REQUIRE( *i1 == 1 );
        delete i1;
    }

    {
        boost::shared_ptr< int > i2;
        BOOST_TEST_REQUIRE( f.pop( i2 ) );
        BOOST_TEST_REQUIRE( *i2 == 2 );
    }

    {
        std::unique_ptr< int > i3;
        BOOST_TEST_REQUIRE( f.pop( i3 ) );
        BOOST_TEST_REQUIRE( *i3 == 3 );
    }

    {
        std::shared_ptr< int > i4;
        BOOST_TEST_REQUIRE( f.pop( i4 ) );
        BOOST_TEST_REQUIRE( *i4 == 4 );
    }

    BOOST_TEST_REQUIRE( f.empty() );
}

BOOST_AUTO_TEST_CASE( mpsc_with_allocator )
{
    using queue_t = bounded_ticket_queue< char, allocator< std::allocator< char > >, single_consumer< true > >;

    auto allocator = queue_t::allocator {};

    {
        queue_t q_with_size_and_allocator { 16, allocator };
    }
    {
        queue_t q_with_size_and_allocator { 16, std::allocator< char > {} };
    }
}

BOOST_AUTO_TEST_CASE( mpsc_move_semantics )
{
    bounded_ticket_queue< int, capacity< 128 >, single_consumer< true > > q;

    q.push( 0 );
    q.push( 1 );

    auto two = 2;
    q.push( std::move( two ) );

    int out;
    BOOST_TEST_REQUIRE( q.pop( out ) );
    BOOST_TEST_REQUIRE( out == 0 );

    q.consume_one( []( int one ) {
        BOOST_TEST_REQUIRE( one == 1 );
    } );

    q.consume_all( []( int ) {} );
}

BOOST_AUTO_TEST_CASE( mpsc_empty_pop_test )
{
    bounded_ticket_queue< int, single_consumer< true > > f( 64 );

    int out = 0xDEAD;
    BOOST_TEST_REQUIRE( !f.pop( out ) );
    BOOST_TEST_REQUIRE( !f.consume_one( []( int ) {} ) );
    BOOST_TEST_REQUIRE( f.consume_all( []( int ) {} ) == 0u );
}

BOOST_AUTO_TEST_CASE( mpsc_push_pop_many )
{
    bounded_ticket_queue< int, single_consumer< true > > f( 128 );

    for ( int i = 0; i < 100; ++i )
        BOOST_TEST_REQUIRE( f.push( i ) );

    for ( int i = 0; i < 100; ++i ) {
        int out;
        BOOST_TEST_REQUIRE( f.pop( out ) );
        BOOST_TEST_REQUIRE( out == i );
    }
    BOOST_TEST_REQUIRE( f.empty() );
}

BOOST_AUTO_TEST_CASE( mpsc_move_only_types )
{
    bounded_ticket_queue< std::unique_ptr< int >, capacity< 128 >, single_consumer< true > > q;

    q.push( std::make_unique< int >( 0 ) );
    q.push( std::make_unique< int >( 1 ) );

    auto two = std::make_unique< int >( 2 );
    q.push( std::move( two ) );

    std::unique_ptr< int > out;
    BOOST_TEST_REQUIRE( q.pop( out ) );
    BOOST_TEST_REQUIRE( *out == 0 );

    q.consume_one( []( std::unique_ptr< int > one ) {
        BOOST_TEST_REQUIRE( *one == 1 );
    } );

    q.consume_all( []( std::unique_ptr< int > ) {} );
}

BOOST_AUTO_TEST_CASE( mpsc_wrap_around )
{
    bounded_ticket_queue< int, single_consumer< true > > f( 4 );

    BOOST_TEST_REQUIRE( f.push( 10 ) );
    BOOST_TEST_REQUIRE( f.push( 20 ) );
    BOOST_TEST_REQUIRE( f.push( 30 ) );
    BOOST_TEST_REQUIRE( f.push( 40 ) );
    BOOST_TEST_REQUIRE( !f.push( 50 ) );

    int out;
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 10 );
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 20 );
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 30 );
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 40 );
    BOOST_TEST_REQUIRE( !f.pop( out ) );
    BOOST_TEST_REQUIRE( f.empty() );

    BOOST_TEST_REQUIRE( f.push( 50 ) );
    BOOST_TEST_REQUIRE( f.push( 60 ) );
    BOOST_TEST_REQUIRE( f.push( 70 ) );
    BOOST_TEST_REQUIRE( f.push( 80 ) );
    BOOST_TEST_REQUIRE( !f.push( 90 ) );

    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 50 );
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 60 );
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 70 );
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 80 );
    BOOST_TEST_REQUIRE( !f.pop( out ) );
    BOOST_TEST_REQUIRE( f.empty() );
}

// ---- SPMC (single_producer<true>) ----

BOOST_AUTO_TEST_CASE( spmc_simple_test )
{
    bounded_ticket_queue< int, single_producer< true > > f( 64 );

    BOOST_TEST_WARN( f.is_lock_free() );

    BOOST_TEST_REQUIRE( f.empty() );
    f.push( 1 );
    f.push( 2 );

    int i1( 0 ), i2( 0 );

    BOOST_TEST_REQUIRE( f.pop( i1 ) );
    BOOST_TEST_REQUIRE( i1 == 1 );

    BOOST_TEST_REQUIRE( f.pop( i2 ) );
    BOOST_TEST_REQUIRE( i2 == 2 );
    BOOST_TEST_REQUIRE( f.empty() );
}

BOOST_AUTO_TEST_CASE( spmc_exhausted )
{
    bounded_ticket_queue< int, single_producer< true > > f( 4 );

    BOOST_TEST_REQUIRE( f.push( 1 ) );
    BOOST_TEST_REQUIRE( f.push( 2 ) );
    BOOST_TEST_REQUIRE( f.push( 3 ) );
    BOOST_TEST_REQUIRE( f.push( 4 ) );
    BOOST_TEST_REQUIRE( !f.push( 5 ) );

    int out;
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 1 );
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 2 );
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 3 );
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 4 );
    BOOST_TEST_REQUIRE( !f.pop( out ) );
    BOOST_TEST_REQUIRE( f.empty() );
}

BOOST_AUTO_TEST_CASE( spmc_consume_all_test )
{
    bounded_ticket_queue< int, single_producer< true > > f( 64 );

    BOOST_TEST_REQUIRE( f.empty() );

    f.push( 1 );
    f.push( 2 );

    size_t consumed = f.consume_all( []( int i ) {} );

    BOOST_TEST_REQUIRE( consumed == 2u );
    BOOST_TEST_REQUIRE( f.empty() );
}

BOOST_AUTO_TEST_CASE( spmc_push_pop_many )
{
    bounded_ticket_queue< int, single_producer< true > > f( 128 );

    for ( int i = 0; i < 100; ++i )
        BOOST_TEST_REQUIRE( f.push( i ) );

    for ( int i = 0; i < 100; ++i ) {
        int out;
        BOOST_TEST_REQUIRE( f.pop( out ) );
        BOOST_TEST_REQUIRE( out == i );
    }
    BOOST_TEST_REQUIRE( f.empty() );
}

BOOST_AUTO_TEST_CASE( spmc_wrap_around )
{
    bounded_ticket_queue< int, single_producer< true > > f( 4 );

    BOOST_TEST_REQUIRE( f.push( 10 ) );
    BOOST_TEST_REQUIRE( f.push( 20 ) );
    BOOST_TEST_REQUIRE( f.push( 30 ) );
    BOOST_TEST_REQUIRE( f.push( 40 ) );
    BOOST_TEST_REQUIRE( !f.push( 50 ) );

    int out;
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 10 );
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 20 );
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 30 );
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 40 );
    BOOST_TEST_REQUIRE( !f.pop( out ) );
    BOOST_TEST_REQUIRE( f.empty() );

    BOOST_TEST_REQUIRE( f.push( 50 ) );
    BOOST_TEST_REQUIRE( f.push( 60 ) );
    BOOST_TEST_REQUIRE( f.push( 70 ) );
    BOOST_TEST_REQUIRE( f.push( 80 ) );
    BOOST_TEST_REQUIRE( !f.push( 90 ) );

    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 50 );
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 60 );
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 70 );
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 80 );
    BOOST_TEST_REQUIRE( !f.pop( out ) );
    BOOST_TEST_REQUIRE( f.empty() );
}

// ---- SPSC (single_producer<true>, single_consumer<true>) ----

BOOST_AUTO_TEST_CASE( spsc_simple_test )
{
    bounded_ticket_queue< int, single_producer< true >, single_consumer< true > > f( 64 );

    BOOST_TEST_WARN( f.is_lock_free() );

    BOOST_TEST_REQUIRE( f.empty() );
    f.push( 1 );
    f.push( 2 );

    int i1( 0 ), i2( 0 );

    BOOST_TEST_REQUIRE( f.pop( i1 ) );
    BOOST_TEST_REQUIRE( i1 == 1 );

    BOOST_TEST_REQUIRE( f.pop( i2 ) );
    BOOST_TEST_REQUIRE( i2 == 2 );
    BOOST_TEST_REQUIRE( f.empty() );
}

BOOST_AUTO_TEST_CASE( spsc_simple_test_capacity )
{
    bounded_ticket_queue< int, capacity< 64 >, single_producer< true >, single_consumer< true > > f;

    BOOST_TEST_WARN( f.is_lock_free() );

    BOOST_TEST_REQUIRE( f.empty() );
    f.push( 1 );
    f.push( 2 );

    int i1( 0 ), i2( 0 );

    BOOST_TEST_REQUIRE( f.pop( i1 ) );
    BOOST_TEST_REQUIRE( i1 == 1 );

    BOOST_TEST_REQUIRE( f.pop( i2 ) );
    BOOST_TEST_REQUIRE( i2 == 2 );
    BOOST_TEST_REQUIRE( f.empty() );
}

BOOST_AUTO_TEST_CASE( spsc_exhausted )
{
    bounded_ticket_queue< int, single_producer< true >, single_consumer< true > > f( 4 );

    BOOST_TEST_REQUIRE( f.push( 1 ) );
    BOOST_TEST_REQUIRE( f.push( 2 ) );
    BOOST_TEST_REQUIRE( f.push( 3 ) );
    BOOST_TEST_REQUIRE( f.push( 4 ) );
    BOOST_TEST_REQUIRE( !f.push( 5 ) );

    int out;
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 1 );
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 2 );
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 3 );
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 4 );
    BOOST_TEST_REQUIRE( !f.pop( out ) );
    BOOST_TEST_REQUIRE( f.empty() );
}

BOOST_AUTO_TEST_CASE( spsc_consume_all_test )
{
    bounded_ticket_queue< int, single_producer< true >, single_consumer< true > > f( 64 );

    BOOST_TEST_REQUIRE( f.empty() );

    f.push( 1 );
    f.push( 2 );

    size_t consumed = f.consume_all( []( int i ) {} );

    BOOST_TEST_REQUIRE( consumed == 2u );
    BOOST_TEST_REQUIRE( f.empty() );
}

BOOST_AUTO_TEST_CASE( spsc_push_pop_many )
{
    bounded_ticket_queue< int, single_producer< true >, single_consumer< true > > f( 128 );

    for ( int i = 0; i < 100; ++i )
        BOOST_TEST_REQUIRE( f.push( i ) );

    for ( int i = 0; i < 100; ++i ) {
        int out;
        BOOST_TEST_REQUIRE( f.pop( out ) );
        BOOST_TEST_REQUIRE( out == i );
    }
    BOOST_TEST_REQUIRE( f.empty() );
}

BOOST_AUTO_TEST_CASE( spsc_wrap_around )
{
    bounded_ticket_queue< int, single_producer< true >, single_consumer< true > > f( 4 );

    BOOST_TEST_REQUIRE( f.push( 10 ) );
    BOOST_TEST_REQUIRE( f.push( 20 ) );
    BOOST_TEST_REQUIRE( f.push( 30 ) );
    BOOST_TEST_REQUIRE( f.push( 40 ) );
    BOOST_TEST_REQUIRE( !f.push( 50 ) );

    int out;
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 10 );
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 20 );
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 30 );
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 40 );
    BOOST_TEST_REQUIRE( !f.pop( out ) );
    BOOST_TEST_REQUIRE( f.empty() );

    BOOST_TEST_REQUIRE( f.push( 50 ) );
    BOOST_TEST_REQUIRE( f.push( 60 ) );
    BOOST_TEST_REQUIRE( f.push( 70 ) );
    BOOST_TEST_REQUIRE( f.push( 80 ) );
    BOOST_TEST_REQUIRE( !f.push( 90 ) );

    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 50 );
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 60 );
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 70 );
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 80 );
    BOOST_TEST_REQUIRE( !f.pop( out ) );
    BOOST_TEST_REQUIRE( f.empty() );
}
