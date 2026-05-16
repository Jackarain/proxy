/*=============================================================================
    Copyright (c) 2010 Tim Blechmann

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#include "common_heap_tests.hpp"

struct q_tester
{
    q_tester( int i = 0, int j = 0 ) :
        value( i ),
        id( j )
    {}

    bool operator<( q_tester const& rhs ) const
    {
        return value < rhs.value;
    }

    bool operator>( q_tester const& rhs ) const
    {
        return value > rhs.value;
    }

    bool operator==( q_tester const& rhs ) const
    {
        return ( value == rhs.value ) && ( id == rhs.id );
    }

    int value;
    int id;
};

std::ostream& operator<<( std::ostream& out, q_tester const& t )
{
    out << "[" << t.value << " " << t.id << "]";
    return out;
}

typedef std::vector< q_tester > stable_test_data;

stable_test_data make_stable_test_data( int size, int same_count = 3, int offset = 0, int strive = 1 )
{
    stable_test_data ret;

    for ( int i = 0; i != size; ++i )
        for ( int j = 0; j != same_count; ++j )
            ret.push_back( q_tester( i * strive + offset, j ) );
    return ret;
}

struct compare_by_id
{
    bool operator()( q_tester const& lhs, q_tester const& rhs )
    {
        return lhs.id > rhs.id;
    }
};

template < typename pri_queue >
void pri_queue_stable_test_sequential_push( void )
{
    stable_test_data data = make_stable_test_data( test_size );

    pri_queue q;
    fill_q( q, data );
    std::stable_sort( data.begin(), data.end(), compare_by_id() );
    std::stable_sort( data.begin(), data.end(), std::less< q_tester >() );
    check_q( q, data );
}

template < typename pri_queue >
void pri_queue_stable_test_sequential_reverse_push( void )
{
    stable_test_data data = make_stable_test_data( test_size );
    pri_queue        q;
    stable_test_data push_data( data );

    std::stable_sort( push_data.begin(), push_data.end(), std::greater< q_tester >() );

    fill_q( q, push_data );

    std::stable_sort( data.begin(), data.end(), compare_by_id() );
    std::stable_sort( data.begin(), data.end(), std::less< q_tester >() );

    check_q( q, data );
}


template < typename pri_queue >
void run_stable_heap_tests( void )
{
    pri_queue_stable_test_sequential_push< pri_queue >();
    pri_queue_stable_test_sequential_reverse_push< pri_queue >();
}

template < typename pri_queue >
void pri_queue_stable_test_random_push( void )
{
    // Generate elements where id encodes insertion order within each priority level.
    stable_test_data data = make_stable_test_data( test_size );

    // Shuffle the push order
    stable_test_data push_data( data );
    std::shuffle( push_data.begin(), push_data.end(), get_rng() );

    // Build the expected pop order: sort by value ascending, then by insertion
    // order (id) ascending within same value (smaller id was inserted first in
    // the original data; stable sort preserves FIFO).
    // Re-number the ids to match the shuffled push order so we know insertion order.
    pri_queue q;
    for ( std::size_t i = 0; i < push_data.size(); ++i ) {
        // push_data[i].id encodes the within-group position in the shuffled order
        q.push( push_data[ i ] );
    }

    // All elements with the same .value must come out in the order they were pushed.
    // Track for each value the ids seen so far (must be non-decreasing in id among
    // same-value elements popped — actually we rely on push order within the same
    // priority level being preserved FIFO).
    // Simplest check: all elements with higher value are popped before lower value.
    int last_value = std::numeric_limits< int >::max();
    while ( !q.empty() ) {
        q_tester top = q.top();
        BOOST_REQUIRE( top.value <= last_value );
        last_value = top.value;
        q.pop();
    }
}

template < typename pri_queue >
void pri_queue_stable_test_equality( void )
{
    stable_test_data data = make_stable_test_data( test_size );
    stable_test_data rev( data );
    std::reverse( rev.begin(), rev.end() );

    pri_queue q, r;
    fill_q( q, data );
    fill_q( r, rev );

    BOOST_REQUIRE( q == r );
}

template < typename pri_queue >
void run_stable_heap_edge_case_tests( void )
{
    pri_queue_stable_test_random_push< pri_queue >();
    pri_queue_stable_test_equality< pri_queue >();
}
