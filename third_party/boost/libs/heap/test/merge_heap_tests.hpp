/*=============================================================================
    Copyright (c) 2010 Tim Blechmann

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#include "common_heap_tests.hpp"
#include <boost/heap/heap_merge.hpp>

#define GENERATE_TEST_DATA( INDEX )                         \
    test_data data = make_test_data( test_size, 0, 1 );     \
    std::shuffle( data.begin(), data.end(), get_rng() );    \
                                                            \
    test_data data_q( data.begin(), data.begin() + INDEX ); \
    test_data data_r( data.begin() + INDEX, data.end() );   \
                                                            \
    std::stable_sort( data.begin(), data.end() );

template < typename pri_queue >
struct pri_queue_test_merge
{
    static void run( void )
    {
        for ( int i = 0; i != test_size; ++i ) {
            pri_queue q, r;
            GENERATE_TEST_DATA( i );

            fill_q( q, data_q );
            fill_q( r, data_r );

            q.merge( r );

            BOOST_REQUIRE( r.empty() );
            check_q( q, data );
        }
    }
};


template < typename pri_queue1, typename pri_queue2 >
struct pri_queue_test_heap_merge
{
    static void run( void )
    {
        for ( int i = 0; i != test_size; ++i ) {
            pri_queue1 q;
            pri_queue2 r;
            GENERATE_TEST_DATA( i );

            fill_q( q, data_q );
            fill_q( r, data_r );

            boost::heap::heap_merge( q, r );

            BOOST_REQUIRE( r.empty() );
            check_q( q, data );
        }
    }
};


template < typename pri_queue >
void run_merge_tests( void )
{
    std::conditional< pri_queue::is_mergable, pri_queue_test_merge< pri_queue >, dummy_run >::type::run();

    pri_queue_test_heap_merge< pri_queue, pri_queue >::run();
}

template < typename pri_queue >
void pri_queue_test_merge_empty_rhs_impl( std::true_type /*is_mergable*/ )
{
    pri_queue q, r;
    test_data data = make_test_data( test_size );
    fill_q( q, data );

    q.merge( r ); // r is empty

    BOOST_REQUIRE( r.empty() );
    std::sort( data.begin(), data.end() );
    check_q( q, data );
}

template < typename pri_queue >
void pri_queue_test_merge_empty_rhs_impl( std::false_type )
{}

template < typename pri_queue >
void pri_queue_test_merge_empty_rhs( void )
{
    pri_queue_test_merge_empty_rhs_impl< pri_queue >( std::integral_constant< bool, pri_queue::is_mergable >() );
}

template < typename pri_queue >
void pri_queue_test_merge_empty_lhs_impl( std::true_type )
{
    pri_queue q, r;
    test_data data = make_test_data( test_size );
    fill_q( r, data );

    q.merge( r ); // q is empty

    BOOST_REQUIRE( r.empty() );
    std::sort( data.begin(), data.end() );
    check_q( q, data );
}

template < typename pri_queue >
void pri_queue_test_merge_empty_lhs_impl( std::false_type )
{}

template < typename pri_queue >
void pri_queue_test_merge_empty_lhs( void )
{
    pri_queue_test_merge_empty_lhs_impl< pri_queue >( std::integral_constant< bool, pri_queue::is_mergable >() );
}

template < typename pri_queue >
void pri_queue_test_heap_merge_empty_lhs( void )
{
    pri_queue q, r;
    test_data data = make_test_data( test_size );
    fill_q( r, data );

    boost::heap::heap_merge( q, r );

    BOOST_REQUIRE( r.empty() );
    std::sort( data.begin(), data.end() );
    check_q( q, data );
}

template < typename pri_queue >
void pri_queue_test_heap_merge_empty_rhs( void )
{
    pri_queue q, r;
    test_data data = make_test_data( test_size );
    fill_q( q, data );

    boost::heap::heap_merge( q, r );

    BOOST_REQUIRE( r.empty() );
    std::sort( data.begin(), data.end() );
    check_q( q, data );
}

template < typename pri_queue >
void pri_queue_test_merge_single_elements_impl( std::true_type )
{
    pri_queue q, r;
    q.push( 3 );
    r.push( 7 );
    q.merge( r );

    BOOST_REQUIRE( r.empty() );
    BOOST_REQUIRE_EQUAL( q.size(), 2u );
    BOOST_REQUIRE_EQUAL( q.top(), 7 );
    q.pop();
    BOOST_REQUIRE_EQUAL( q.top(), 3 );
    q.pop();
    BOOST_REQUIRE( q.empty() );
}

template < typename pri_queue >
void pri_queue_test_merge_single_elements_impl( std::false_type )
{}

template < typename pri_queue >
void pri_queue_test_merge_single_elements( void )
{
    pri_queue_test_merge_single_elements_impl< pri_queue >( std::integral_constant< bool, pri_queue::is_mergable >() );
}

template < typename pri_queue >
void pri_queue_test_heap_merge_correctness_after_updates( void )
{
    // Build q with 0..N-1, build r with N..2N-1, merge r into q.
    pri_queue q, r;
    test_data data_q = make_test_data( test_size );
    test_data data_r = make_test_data( test_size, test_size ); // N..2N-1

    fill_q( q, data_q );
    fill_q( r, data_r );

    boost::heap::heap_merge( q, r );

    BOOST_REQUIRE( r.empty() );
    BOOST_REQUIRE_EQUAL( q.size(), ( typename pri_queue::size_type )( test_size * 2 ) );

    // Pop all — should come out in descending order 2N-1..0
    int prev = test_size * 2;
    while ( !q.empty() ) {
        int v = q.top();
        q.pop();
        BOOST_REQUIRE( v < prev );
        prev = v;
    }
}

template < typename pri_queue >
void run_merge_edge_case_tests( void )
{
    pri_queue_test_merge_empty_rhs< pri_queue >();
    pri_queue_test_merge_empty_lhs< pri_queue >();
    pri_queue_test_heap_merge_empty_lhs< pri_queue >();
    pri_queue_test_heap_merge_empty_rhs< pri_queue >();
    pri_queue_test_merge_single_elements< pri_queue >();
    pri_queue_test_heap_merge_correctness_after_updates< pri_queue >();
}
