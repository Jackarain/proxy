/*=============================================================================
    Copyright (c) 2010 Tim Blechmann

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#include <random>

#include "common_heap_tests.hpp"


#define PUSH_WITH_HANDLES( HANDLES, Q, DATA )               \
    std::vector< typename pri_queue::handle_type > HANDLES; \
                                                            \
    for ( unsigned int k = 0; k != data.size(); ++k )       \
        HANDLES.push_back( Q.push( DATA[ k ] ) );


template < typename pri_queue >
void pri_queue_test_update_decrease( void )
{
    for ( int i = 0; i != test_size; ++i ) {
        pri_queue q;
        test_data data = make_test_data( test_size );
        PUSH_WITH_HANDLES( handles, q, data );

        *handles[ i ] = -1;
        data[ i ]     = -1;
        q.update( handles[ i ] );

        std::sort( data.begin(), data.end() );
        check_q( q, data );
    }
}

template < typename pri_queue >
void pri_queue_test_update_decrease_function( void )
{
    for ( int i = 0; i != test_size; ++i ) {
        pri_queue q;
        test_data data = make_test_data( test_size );
        PUSH_WITH_HANDLES( handles, q, data );

        data[ i ] = -1;
        q.update( handles[ i ], -1 );

        std::sort( data.begin(), data.end() );
        check_q( q, data );
    }
}

template < typename pri_queue >
void pri_queue_test_update_function_identity( void )
{
    for ( int i = 0; i != test_size; ++i ) {
        pri_queue q;
        test_data data = make_test_data( test_size );
        PUSH_WITH_HANDLES( handles, q, data );

        q.update( handles[ i ], data[ i ] );

        std::sort( data.begin(), data.end() );
        check_q( q, data );
    }
}

template < typename pri_queue >
void pri_queue_test_update_shuffled( void )
{
    pri_queue q;
    test_data data = make_test_data( test_size );
    PUSH_WITH_HANDLES( handles, q, data );

    test_data shuffled( data );
    std::shuffle( shuffled.begin(), shuffled.end(), get_rng() );

    for ( int i = 0; i != test_size; ++i )
        q.update( handles[ i ], shuffled[ i ] );

    check_q( q, data );
}


template < typename pri_queue >
void pri_queue_test_update_increase( void )
{
    for ( int i = 0; i != test_size; ++i ) {
        pri_queue q;
        test_data data = make_test_data( test_size );
        PUSH_WITH_HANDLES( handles, q, data );

        data[ i ]     = data.back() + 1;
        *handles[ i ] = data[ i ];
        q.update( handles[ i ] );

        std::sort( data.begin(), data.end() );
        check_q( q, data );
    }
}

template < typename pri_queue >
void pri_queue_test_update_increase_function( void )
{
    for ( int i = 0; i != test_size; ++i ) {
        pri_queue q;
        test_data data = make_test_data( test_size );
        PUSH_WITH_HANDLES( handles, q, data );

        data[ i ] = data.back() + 1;
        q.update( handles[ i ], data[ i ] );

        std::sort( data.begin(), data.end() );
        check_q( q, data );
    }
}

template < typename pri_queue >
void pri_queue_test_decrease( void )
{
    for ( int i = 0; i != test_size; ++i ) {
        pri_queue q;
        test_data data = make_test_data( test_size );
        PUSH_WITH_HANDLES( handles, q, data );

        *handles[ i ] = -1;
        data[ i ]     = -1;
        q.decrease( handles[ i ] );

        std::sort( data.begin(), data.end() );
        check_q( q, data );
    }
}

template < typename pri_queue >
void pri_queue_test_decrease_function( void )
{
    for ( int i = 0; i != test_size; ++i ) {
        pri_queue q;
        test_data data = make_test_data( test_size );
        PUSH_WITH_HANDLES( handles, q, data );

        data[ i ] = -1;
        q.decrease( handles[ i ], -1 );

        std::sort( data.begin(), data.end() );
        check_q( q, data );
    }
}

template < typename pri_queue >
void pri_queue_test_decrease_function_identity( void )
{
    for ( int i = 0; i != test_size; ++i ) {
        pri_queue q;
        test_data data = make_test_data( test_size );
        PUSH_WITH_HANDLES( handles, q, data );

        q.decrease( handles[ i ], data[ i ] );

        check_q( q, data );
    }
}


template < typename pri_queue >
void pri_queue_test_increase( void )
{
    for ( int i = 0; i != test_size; ++i ) {
        pri_queue q;
        test_data data = make_test_data( test_size );
        PUSH_WITH_HANDLES( handles, q, data );

        data[ i ]     = data.back() + 1;
        *handles[ i ] = data[ i ];
        q.increase( handles[ i ] );

        std::sort( data.begin(), data.end() );
        check_q( q, data );
    }
}

template < typename pri_queue >
void pri_queue_test_increase_function( void )
{
    for ( int i = 0; i != test_size; ++i ) {
        pri_queue q;
        test_data data = make_test_data( test_size );
        PUSH_WITH_HANDLES( handles, q, data );

        data[ i ] = data.back() + 1;
        q.increase( handles[ i ], data[ i ] );

        std::sort( data.begin(), data.end() );
        check_q( q, data );
    }
}

template < typename pri_queue >
void pri_queue_test_increase_function_identity( void )
{
    for ( int i = 0; i != test_size; ++i ) {
        pri_queue q;
        test_data data = make_test_data( test_size );
        PUSH_WITH_HANDLES( handles, q, data );

        q.increase( handles[ i ], data[ i ] );

        check_q( q, data );
    }
}

template < typename pri_queue >
void pri_queue_test_erase( void )
{
    for ( int i = 0; i != test_size; ++i ) {
        pri_queue q;
        test_data data = make_test_data( test_size );
        PUSH_WITH_HANDLES( handles, q, data );

        for ( int j = 0; j != i; ++j ) {
            std::uniform_int_distribution<> range( 0, data.size() - 1 );

            int index = range( get_rng() );
            q.erase( handles[ index ] );
            handles.erase( handles.begin() + index );
            data.erase( data.begin() + index );
        }

        std::sort( data.begin(), data.end() );
        check_q( q, data );
    }
}


template < typename pri_queue >
void run_mutable_heap_update_tests( void )
{
    pri_queue_test_update_increase< pri_queue >();
    pri_queue_test_update_decrease< pri_queue >();

    pri_queue_test_update_shuffled< pri_queue >();
}

template < typename pri_queue >
void run_mutable_heap_update_function_tests( void )
{
    pri_queue_test_update_increase_function< pri_queue >();
    pri_queue_test_update_decrease_function< pri_queue >();
    pri_queue_test_update_function_identity< pri_queue >();
}


template < typename pri_queue >
void run_mutable_heap_decrease_tests( void )
{
    pri_queue_test_decrease< pri_queue >();
    pri_queue_test_decrease_function< pri_queue >();
    pri_queue_test_decrease_function_identity< pri_queue >();
}

template < typename pri_queue >
void run_mutable_heap_increase_tests( void )
{
    pri_queue_test_increase< pri_queue >();
    pri_queue_test_increase_function< pri_queue >();
    pri_queue_test_increase_function_identity< pri_queue >();
}

template < typename pri_queue >
void run_mutable_heap_erase_tests( void )
{
    pri_queue_test_erase< pri_queue >();
}

template < typename pri_queue >
void run_mutable_heap_test_handle_from_iterator( void )
{
    pri_queue que;

    que.push( 3 );
    que.push( 1 );
    que.push( 4 );

    que.update( pri_queue::s_handle_from_iterator( que.begin() ), 6 );
}


template < typename pri_queue >
void run_mutable_heap_tests( void )
{
    run_mutable_heap_update_function_tests< pri_queue >();
    run_mutable_heap_update_tests< pri_queue >();
    run_mutable_heap_decrease_tests< pri_queue >();
    run_mutable_heap_increase_tests< pri_queue >();
    run_mutable_heap_erase_tests< pri_queue >();
    run_mutable_heap_test_handle_from_iterator< pri_queue >();
}

template < typename pri_queue >
void run_ordered_iterator_tests()
{
    pri_queue_test_ordered_iterators< pri_queue >();
}

template < typename pri_queue >
void pri_queue_test_update_top_decrease( void )
{
    pri_queue q;
    test_data data = make_test_data( test_size );
    PUSH_WITH_HANDLES( handles, q, data );

    // The top element has value test_size-1 (maximum).  Decrease it to -1.
    int top_index         = test_size - 1;
    *handles[ top_index ] = -1;
    data[ top_index ]     = -1;
    q.update( handles[ top_index ] );

    std::sort( data.begin(), data.end() );
    check_q( q, data );
}

template < typename pri_queue >
void pri_queue_test_update_top_increase( void )
{
    pri_queue q;
    test_data data = make_test_data( test_size );
    PUSH_WITH_HANDLES( handles, q, data );

    int top_index     = test_size - 1;
    int new_val       = data.back() + 100;
    data[ top_index ] = new_val;
    q.update( handles[ top_index ], new_val );

    std::sort( data.begin(), data.end() );
    check_q( q, data );
}

template < typename pri_queue >
void pri_queue_test_increase_top( void )
{
    pri_queue q;
    test_data data = make_test_data( test_size );
    PUSH_WITH_HANDLES( handles, q, data );

    int top_index         = test_size - 1;
    int new_val           = data.back() + 50;
    data[ top_index ]     = new_val;
    *handles[ top_index ] = new_val;
    q.increase( handles[ top_index ] );

    std::sort( data.begin(), data.end() );
    check_q( q, data );
}

template < typename pri_queue >
void pri_queue_test_decrease_top( void )
{
    pri_queue q;
    test_data data = make_test_data( test_size );
    PUSH_WITH_HANDLES( handles, q, data );

    int top_index         = test_size - 1;
    data[ top_index ]     = -99;
    *handles[ top_index ] = -99;
    q.decrease( handles[ top_index ] );

    std::sort( data.begin(), data.end() );
    check_q( q, data );
}

template < typename pri_queue >
void pri_queue_test_erase_top( void )
{
    for ( int i = 1; i != test_size; ++i ) {
        pri_queue q;
        test_data data = make_test_data( i );
        PUSH_WITH_HANDLES( handles, q, data );

        // top element is the last in 'data' (= i-1)
        q.erase( handles[ i - 1 ] );
        data.erase( data.begin() + ( i - 1 ) );

        std::sort( data.begin(), data.end() );
        check_q( q, data );
    }
}

template < typename pri_queue >
void pri_queue_test_erase_all( void )
{
    pri_queue q;
    test_data data = make_test_data( test_size );
    PUSH_WITH_HANDLES( handles, q, data );

    for ( int i = 0; i < test_size; ++i )
        q.erase( handles[ i ] );

    BOOST_REQUIRE( q.empty() );
    BOOST_REQUIRE_EQUAL( q.size(), 0u );
}

template < typename pri_queue >
void pri_queue_test_multiple_updates_same_handle( void )
{
    pri_queue q;
    test_data data = make_test_data( test_size );
    PUSH_WITH_HANDLES( handles, q, data );

    // Update handle[0] multiple times
    q.update( handles[ 0 ], 100 );
    data[ 0 ] = 100;
    q.update( handles[ 0 ], 200 );
    data[ 0 ] = 200;
    q.update( handles[ 0 ], -5 );
    data[ 0 ] = -5;

    std::sort( data.begin(), data.end() );
    check_q( q, data );
}

template < typename pri_queue >
void pri_queue_test_mutable_copy_constructor( void )
{
    for ( int i = 1; i != test_size; ++i ) {
        pri_queue q;
        test_data data = make_test_data( i );
        fill_q( q, data );

        pri_queue r( q ); // copy

        // Both heaps must have the same size and produce the same sequence
        BOOST_REQUIRE_EQUAL( q.size(), r.size() );
        while ( !q.empty() ) {
            BOOST_REQUIRE_EQUAL( q.top(), r.top() );
            q.pop();
            r.pop();
        }
        BOOST_REQUIRE( r.empty() );
    }
}

template < typename pri_queue >
void pri_queue_test_mutable_copy_independence( void )
{
    pri_queue q;
    test_data data = make_test_data( test_size );
    fill_q( q, data );

    pri_queue r( q );

    // Modify r via its own handles (obtained by iterating)
    for ( auto it = r.begin(); it != r.end(); ++it ) {
        auto h = pri_queue::s_handle_from_iterator( it );
        r.update( h, *h + 1000 );
        break; // just change the first one found
    }

    // q should still have the original top
    std::sort( data.begin(), data.end() );
    check_q( q, data );
}

template < typename pri_queue >
void run_mutable_heap_edge_case_tests( void )
{
    pri_queue_test_update_top_decrease< pri_queue >();
    pri_queue_test_update_top_increase< pri_queue >();
    pri_queue_test_increase_top< pri_queue >();
    pri_queue_test_decrease_top< pri_queue >();
    pri_queue_test_erase_top< pri_queue >();
    pri_queue_test_erase_all< pri_queue >();
    pri_queue_test_multiple_updates_same_handle< pri_queue >();
    pri_queue_test_mutable_copy_constructor< pri_queue >();
    pri_queue_test_mutable_copy_independence< pri_queue >();
}
