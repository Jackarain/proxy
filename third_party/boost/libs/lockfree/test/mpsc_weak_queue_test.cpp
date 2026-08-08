//  Copyright (C) 2024 Tim Blechmann
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/lockfree/lockfree_forward.hpp>

#include <boost/lockfree/mpsc_weak_queue.hpp>

#define BOOST_TEST_MAIN
#ifdef BOOST_LOCKFREE_INCLUDE_TESTS
#    include <boost/test/included/unit_test.hpp>
#else
#    include <boost/test/unit_test.hpp>
#endif

#include <memory>


using namespace boost::lockfree;

BOOST_AUTO_TEST_CASE( simple_mpsc_weak_queue_test )
{
    mpsc_weak_queue< int > f( 64 );

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

BOOST_AUTO_TEST_CASE( simple_mpsc_weak_queue_test_capacity )
{
    mpsc_weak_queue< int, capacity< 64 > > f;

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


BOOST_AUTO_TEST_CASE( unsafe_mpsc_weak_queue_test )
{
    mpsc_weak_queue< int > f( 64 );

    BOOST_TEST_WARN( f.is_lock_free() );
    BOOST_TEST_REQUIRE( f.empty() );

    int i1( 0 ), i2( 0 );

    f.unsynchronized_push( 1 );
    f.unsynchronized_push( 2 );

    BOOST_TEST_REQUIRE( f.unsynchronized_pop( i1 ) );
    BOOST_TEST_REQUIRE( i1 == 1 );

    BOOST_TEST_REQUIRE( f.unsynchronized_pop( i2 ) );
    BOOST_TEST_REQUIRE( i2 == 2 );
    BOOST_TEST_REQUIRE( f.empty() );
}


BOOST_AUTO_TEST_CASE( mpsc_weak_queue_consume_one_test )
{
    mpsc_weak_queue< int > f( 64 );

    BOOST_TEST_WARN( f.is_lock_free() );
    BOOST_TEST_REQUIRE( f.empty() );

    f.push( 1 );
    f.push( 2 );

    bool success1 = f.consume_one( []( int i ) {
        BOOST_TEST_REQUIRE( i == 1 );
    } );

    bool success2 = f.consume_one( []( int i ) mutable {
        BOOST_TEST_REQUIRE( i == 2 );
    } );

    BOOST_TEST_REQUIRE( success1 );
    BOOST_TEST_REQUIRE( success2 );

    BOOST_TEST_REQUIRE( f.empty() );
}

BOOST_AUTO_TEST_CASE( mpsc_weak_queue_consume_all_test )
{
    mpsc_weak_queue< int > f( 64 );

    BOOST_TEST_WARN( f.is_lock_free() );
    BOOST_TEST_REQUIRE( f.empty() );

    f.push( 1 );
    f.push( 2 );

    size_t consumed = f.consume_all( []( int i ) {} );

    BOOST_TEST_REQUIRE( consumed == 2u );

    BOOST_TEST_REQUIRE( f.empty() );
}


BOOST_AUTO_TEST_CASE( mpsc_weak_queue_convert_pop_test )
{
    mpsc_weak_queue< int* > f( 128 );
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

BOOST_AUTO_TEST_CASE( reserve_test )
{
    typedef boost::lockfree::mpsc_weak_queue< void* > memory_queue;

    memory_queue ms( 1 );
    ms.reserve( 1 );
    ms.reserve_unsafe( 1 );
}

BOOST_AUTO_TEST_CASE( mpsc_weak_queue_with_allocator )
{
    using allocator_type = std::allocator< char >;

    using queue_t = boost::lockfree::mpsc_weak_queue< char, boost::lockfree::allocator< allocator_type > >;
    using queue_with_capacity_t = boost::lockfree::
        mpsc_weak_queue< char, boost::lockfree::allocator< allocator_type >, boost::lockfree::capacity< 16 > >;

    auto allocator = queue_t::allocator {};

    {
        queue_with_capacity_t q_with_allocator {
            allocator,
        };
        queue_t q_with_size_and_allocator {
            5,
            allocator,
        };
    }
    {
        queue_with_capacity_t q_with_allocator {
            allocator_type {},
        };
        queue_t q_with_size_and_allocator {
            5,
            allocator_type {},
        };
    }
}

BOOST_AUTO_TEST_CASE( move_semantics )
{
    boost::lockfree::mpsc_weak_queue< int, boost::lockfree::capacity< 128 > > stk;

    stk.push( 0 );
    stk.push( 1 );

    auto two = 2;
    stk.push( std::move( two ) );

    int out;
    BOOST_TEST_REQUIRE( stk.pop( out ) );
    BOOST_TEST_REQUIRE( out == 0 );

    stk.consume_one( []( int one ) {
        BOOST_TEST_REQUIRE( one == 1 );
    } );

    stk.consume_all( []( int ) {} );
}

#if !defined( BOOST_NO_CXX17_HDR_OPTIONAL )

BOOST_AUTO_TEST_CASE( mpsc_weak_queue_uses_optional )
{
    boost::lockfree::mpsc_weak_queue< int > stk( 5 );

    bool pop_to_nullopt = stk.pop( boost::lockfree::uses_optional ) == std::nullopt;
    BOOST_TEST_REQUIRE( pop_to_nullopt );

    stk.push( 53 );
    bool pop_to_optional = stk.pop( boost::lockfree::uses_optional ) == 53;
    BOOST_TEST_REQUIRE( pop_to_optional );
}

BOOST_AUTO_TEST_CASE( mpsc_weak_queue_uses_optional_capacity )
{
    boost::lockfree::mpsc_weak_queue< int, boost::lockfree::capacity< 64 > > stk;

    bool pop_to_nullopt = stk.pop( boost::lockfree::uses_optional ) == std::nullopt;
    BOOST_TEST_REQUIRE( pop_to_nullopt );

    stk.push( 53 );
    bool pop_to_optional = stk.pop( boost::lockfree::uses_optional ) == 53;
    BOOST_TEST_REQUIRE( pop_to_optional );
}

#endif

BOOST_AUTO_TEST_CASE( fixed_size_mpsc_weak_queue_test_exhausted )
{
    mpsc_weak_queue< int, capacity< 2 >, freelist< true > > f;

    BOOST_TEST_REQUIRE( f.push( 1 ) );
    BOOST_TEST_REQUIRE( f.push( 2 ) );
    BOOST_TEST_REQUIRE( !f.push( 3 ) );

    int out;
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 1 );
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 2 );
    BOOST_TEST_REQUIRE( !f.pop( out ) );
    BOOST_TEST_REQUIRE( f.empty() );
}

BOOST_AUTO_TEST_CASE( bounded_mpsc_weak_queue_test_exhausted )
{
    mpsc_weak_queue< int, freelist< true > > f( 2 );

    BOOST_TEST_REQUIRE( f.bounded_push( 1 ) );
    BOOST_TEST_REQUIRE( f.bounded_push( 2 ) );
    BOOST_TEST_REQUIRE( !f.bounded_push( 3 ) );

    int out;
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 1 );
    BOOST_TEST_REQUIRE( f.pop( out ) );
    BOOST_TEST_REQUIRE( out == 2 );
    BOOST_TEST_REQUIRE( !f.pop( out ) );
    BOOST_TEST_REQUIRE( f.empty() );
}

BOOST_AUTO_TEST_CASE( mpsc_weak_queue_unsynchronized_push_const_ref )
{
    mpsc_weak_queue< int > f( 64 );

    BOOST_TEST_REQUIRE( f.empty() );

    const int a = 42;
    const int b = 43;

    f.unsynchronized_push( a );
    f.unsynchronized_push( b );

    int i1( 0 ), i2( 0 );
    BOOST_TEST_REQUIRE( f.unsynchronized_pop( i1 ) );
    BOOST_TEST_REQUIRE( i1 == 42 );
    BOOST_TEST_REQUIRE( f.unsynchronized_pop( i2 ) );
    BOOST_TEST_REQUIRE( i2 == 43 );
    BOOST_TEST_REQUIRE( f.empty() );
}

BOOST_AUTO_TEST_CASE( mpsc_weak_queue_consume_one_capacity_test )
{
    mpsc_weak_queue< int, capacity< 64 > > f;

    BOOST_TEST_REQUIRE( f.empty() );

    f.push( 10 );
    f.push( 20 );

    bool success1 = f.consume_one( []( int i ) {
        BOOST_TEST_REQUIRE( i == 10 );
    } );

    bool success2 = f.consume_one( []( int i ) {
        BOOST_TEST_REQUIRE( i == 20 );
    } );

    BOOST_TEST_REQUIRE( success1 );
    BOOST_TEST_REQUIRE( success2 );
    BOOST_TEST_REQUIRE( !f.consume_one( []( int ) {} ) );
    BOOST_TEST_REQUIRE( f.empty() );
}

BOOST_AUTO_TEST_CASE( mpsc_weak_queue_consume_all_capacity_test )
{
    mpsc_weak_queue< int, capacity< 64 > > f;

    BOOST_TEST_REQUIRE( f.empty() );

    f.push( 1 );
    f.push( 2 );
    f.push( 3 );

    size_t consumed = f.consume_all( []( int ) {} );

    BOOST_TEST_REQUIRE( consumed == 3u );
    BOOST_TEST_REQUIRE( f.empty() );
}

BOOST_AUTO_TEST_CASE( mpsc_weak_queue_empty_pop_test )
{
    mpsc_weak_queue< int > f( 64 );

    int out = 0xDEAD;
    BOOST_TEST_REQUIRE( !f.pop( out ) );
    BOOST_TEST_REQUIRE( !f.unsynchronized_pop( out ) );
    BOOST_TEST_REQUIRE( !f.consume_one( []( int ) {} ) );
    BOOST_TEST_REQUIRE( f.consume_all( []( int ) {} ) == 0u );
}

BOOST_AUTO_TEST_CASE( mpsc_weak_queue_push_pop_many )
{
    mpsc_weak_queue< int > f( 64 );

    for ( int i = 0; i < 100; ++i )
        BOOST_TEST_REQUIRE( f.push( i ) );

    for ( int i = 0; i < 100; ++i ) {
        int out;
        BOOST_TEST_REQUIRE( f.pop( out ) );
        BOOST_TEST_REQUIRE( out == i );
    }
    BOOST_TEST_REQUIRE( f.empty() );
}

BOOST_AUTO_TEST_CASE( mpsc_weak_queue_push_pop_many_capacity )
{
    mpsc_weak_queue< int, capacity< 128 > > f;

    for ( int i = 0; i < 100; ++i )
        BOOST_TEST_REQUIRE( f.push( i ) );

    for ( int i = 0; i < 100; ++i ) {
        int out;
        BOOST_TEST_REQUIRE( f.pop( out ) );
        BOOST_TEST_REQUIRE( out == i );
    }
    BOOST_TEST_REQUIRE( f.empty() );
}

BOOST_AUTO_TEST_CASE( move_only_types )
{
    boost::lockfree::mpsc_weak_queue< std::unique_ptr< int >, boost::lockfree::capacity< 128 > > stk;

    stk.push( std::make_unique< int >( 0 ) );
    stk.push( std::make_unique< int >( 1 ) );

    auto two = std::make_unique< int >( 2 );
    stk.push( std::move( two ) );

    std::unique_ptr< int > out;
    BOOST_TEST_REQUIRE( stk.pop( out ) );
    BOOST_TEST_REQUIRE( *out == 0 );

    stk.consume_one( []( std::unique_ptr< int > one ) {
        BOOST_TEST_REQUIRE( *one == 1 );
    } );

    stk.consume_all( []( std::unique_ptr< int > ) {} );
}
