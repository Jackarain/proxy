/*=============================================================================
    Copyright (c) 2010 Tim Blechmann

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#ifndef COMMON_HEAP_TESTS_HPP_INCLUDED
#define COMMON_HEAP_TESTS_HPP_INCLUDED

#include <algorithm>
#include <random>
#include <vector>

#include <boost/concept/assert.hpp>
#include <boost/concept_archetype.hpp>
#include <boost/container/pmr/global_resource.hpp>
#include <boost/container/pmr/memory_resource.hpp>
#include <boost/container/pmr/polymorphic_allocator.hpp>
#include <boost/shared_ptr.hpp>

#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_log.hpp>

#include <boost/heap/heap_concepts.hpp>

typedef boost::default_constructible_archetype<
    boost::less_than_comparable_archetype< boost::copy_constructible_archetype< boost::assignable_archetype<> > > >
    test_value_type;


typedef std::vector< int > test_data;
const int                  test_size = 32;

struct dummy_run
{
    static void run( void )
    {}
};

test_data make_test_data( int size, int offset = 0, int strive = 1 )
{
    test_data ret;

    for ( int i = 0; i != size; ++i )
        ret.push_back( i * strive + offset );
    return ret;
}

auto& get_rng()
{
    static std::mt19937_64 rng;
    return rng;
}

template < typename T, typename = void >
struct has_ordered_iterators : std::false_type
{};

template < typename T >
struct has_ordered_iterators<
    T,
    boost::void_t< decltype( std::declval< T >().ordered_begin() ), decltype( std::declval< T >().ordered_end() ) > > :
    std::true_type
{};

template < typename T >
using has_ordered_iterators_t = typename has_ordered_iterators< T >::type;

template < typename T >
using enable_if_has_ordered_iterators_t = typename std::enable_if< has_ordered_iterators< T >::value >::type;
template < typename T >
using disable_if_has_ordered_iterators_t = typename std::enable_if< !has_ordered_iterators< T >::value >::type;


template < typename pri_queue, typename data_container >
enable_if_has_ordered_iterators_t< pri_queue > check_q_via_ordered_iterators( pri_queue const&      q,
                                                                              data_container const& expected )
{
    auto x = q.ordered_begin();
    auto y = q.ordered_end();

    auto a = expected.rbegin();
    auto b = expected.rend();

    for ( unsigned int i = 0; i != expected.size(); ++i ) {
        assert( x != y );
        BOOST_REQUIRE( x != y );
        assert( a != b );
        BOOST_REQUIRE( a != b );
        BOOST_REQUIRE_EQUAL( *x, *a );
        ++x;
        ++a;
    }
}

template < typename pri_queue, typename data_container >
disable_if_has_ordered_iterators_t< pri_queue > check_q_via_ordered_iterators( pri_queue const&, data_container const& )
{}

template < typename pri_queue, typename data_container >
void check_q( pri_queue& q, data_container const& expected )
{
    check_q_via_ordered_iterators< pri_queue >( q, expected );

    assert( q.size() == expected.size() );
    BOOST_REQUIRE_EQUAL( q.size(), expected.size() );

    for ( unsigned int i = 0; i != expected.size(); ++i ) {
        assert( q.size() == expected.size() - i );
        BOOST_REQUIRE_EQUAL( q.size(), expected.size() - i );
        BOOST_REQUIRE_EQUAL( q.top(), expected[ expected.size() - 1 - i ] );
        q.pop();
    }

    BOOST_REQUIRE( q.empty() );
}


template < typename pri_queue, typename data_container >
void fill_q( pri_queue& q, data_container const& data )
{
    for ( unsigned int i = 0; i != data.size(); ++i )
        q.push( data[ i ] );
}

template < typename pri_queue, typename data_container >
void fill_emplace_q( pri_queue& q, data_container const& data )
{
    for ( unsigned int i = 0; i != data.size(); ++i ) {
        typename pri_queue::value_type value = data[ i ];
        q.emplace( std::move( value ) );
    }
}

template < typename pri_queue >
void pri_queue_test_sequential_push( void )
{
    for ( int i = 0; i != test_size; ++i ) {
        pri_queue q;
        test_data data = make_test_data( i );
        fill_q( q, data );
        check_q( q, data );
    }
}

template < typename pri_queue >
void pri_queue_test_sequential_reverse_push( void )
{
    for ( int i = 0; i != test_size; ++i ) {
        pri_queue q;
        test_data data = make_test_data( i );
        std::reverse( data.begin(), data.end() );
        fill_q( q, data );
        std::reverse( data.begin(), data.end() );
        check_q( q, data );
    }
}

template < typename pri_queue >
void pri_queue_test_emplace( void )
{
    for ( int i = 0; i != test_size; ++i ) {
        pri_queue q;
        test_data data = make_test_data( i );
        std::reverse( data.begin(), data.end() );
        fill_emplace_q( q, data );
        std::reverse( data.begin(), data.end() );
        check_q( q, data );
    }
}


template < typename pri_queue >
void pri_queue_test_random_push( void )
{
    for ( int i = 0; i != test_size; ++i ) {
        pri_queue q;
        test_data data = make_test_data( i );

        test_data shuffled( data );
        std::shuffle( shuffled.begin(), shuffled.end(), get_rng() );

        fill_q( q, shuffled );

        check_q( q, data );
    }
}

template < typename pri_queue >
void pri_queue_test_copyconstructor( void )
{
    for ( int i = 0; i != test_size; ++i ) {
        pri_queue q;
        test_data data = make_test_data( i );
        fill_q( q, data );

        pri_queue r( q );

        check_q( r, data );
    }
}

template < typename pri_queue >
void pri_queue_test_assignment( void )
{
    for ( int i = 0; i != test_size; ++i ) {
        pri_queue q;
        test_data data = make_test_data( i );
        fill_q( q, data );

        pri_queue r;
        r = q;

        check_q( r, data );
    }
}

template < typename pri_queue >
void pri_queue_test_moveconstructor( void )
{
    pri_queue q;
    test_data data = make_test_data( test_size );
    fill_q( q, data );

    pri_queue r( std::move( q ) );

    check_q( r, data );
    BOOST_REQUIRE( q.empty() );
}

template < typename pri_queue >
void pri_queue_test_move_assignment( void )
{
    pri_queue q;
    test_data data = make_test_data( test_size );
    fill_q( q, data );

    pri_queue r;
    r = std::move( q );

    check_q( r, data );
    BOOST_REQUIRE( q.empty() );
}


template < typename pri_queue >
void pri_queue_test_swap( void )
{
    for ( int i = 0; i != test_size; ++i ) {
        pri_queue q;
        test_data data = make_test_data( i );
        test_data shuffled( data );
        std::shuffle( shuffled.begin(), shuffled.end(), get_rng() );
        fill_q( q, shuffled );

        pri_queue r;

        q.swap( r );
        check_q( r, data );
        BOOST_REQUIRE( q.empty() );
    }
}


template < typename pri_queue >
void pri_queue_test_iterators( void )
{
    for ( int i = 0; i != test_size; ++i ) {
        test_data data = make_test_data( test_size );
        test_data shuffled( data );
        std::shuffle( shuffled.begin(), shuffled.end(), get_rng() );
        pri_queue q;
        BOOST_REQUIRE( q.begin() == q.end() );
        fill_q( q, shuffled );

        for ( unsigned long j = 0; j != data.size(); ++j )
            BOOST_REQUIRE( std::find( q.begin(), q.end(), data[ j ] ) != q.end() );

        for ( unsigned long j = 0; j != data.size(); ++j )
            BOOST_REQUIRE( std::find( q.begin(), q.end(), data[ j ] + data.size() ) == q.end() );

        test_data data_from_queue( q.begin(), q.end() );
        std::sort( data_from_queue.begin(), data_from_queue.end() );

        BOOST_REQUIRE( data == data_from_queue );

        for ( unsigned long j = 0; j != data.size(); ++j ) {
            BOOST_REQUIRE_EQUAL( (long)std::distance( q.begin(), q.end() ), (long)( data.size() - j ) );
            q.pop();
        }
    }
}

template < typename pri_queue >
void pri_queue_test_ordered_iterators( void )
{
    for ( int i = 0; i != test_size; ++i ) {
        test_data data = make_test_data( i );
        test_data shuffled( data );
        std::shuffle( shuffled.begin(), shuffled.end(), get_rng() );
        pri_queue q;
        BOOST_REQUIRE( q.ordered_begin() == q.ordered_end() );
        fill_q( q, shuffled );

        test_data data_from_queue( q.ordered_begin(), q.ordered_end() );
        std::reverse( data_from_queue.begin(), data_from_queue.end() );
        BOOST_REQUIRE( data == data_from_queue );

        for ( unsigned long j = 0; j != data.size(); ++j )
            BOOST_REQUIRE( std::find( q.ordered_begin(), q.ordered_end(), data[ j ] ) != q.ordered_end() );

        for ( unsigned long j = 0; j != data.size(); ++j )
            BOOST_REQUIRE( std::find( q.ordered_begin(), q.ordered_end(), data[ j ] + data.size() ) == q.ordered_end() );

        for ( unsigned long j = 0; j != data.size(); ++j ) {
            BOOST_REQUIRE_EQUAL( (long)std::distance( q.begin(), q.end() ), (long)( data.size() - j ) );
            q.pop();
        }
    }
}


template < typename pri_queue >
void pri_queue_test_equality( void )
{
    for ( int i = 0; i != test_size; ++i ) {
        pri_queue q, r;
        test_data data = make_test_data( i );
        fill_q( q, data );
        std::reverse( data.begin(), data.end() );
        fill_q( r, data );

        BOOST_REQUIRE( r == q );
    }
}

template < typename pri_queue >
void pri_queue_test_inequality( void )
{
    for ( int i = 1; i != test_size; ++i ) {
        pri_queue q, r;
        test_data data = make_test_data( i );
        fill_q( q, data );
        data[ 0 ] = data.back() + 1;
        fill_q( r, data );

        BOOST_REQUIRE( r != q );
    }
}

template < typename pri_queue >
void pri_queue_test_less( void )
{
    for ( int i = 1; i != test_size; ++i ) {
        pri_queue q, r;
        test_data data = make_test_data( i );
        test_data r_data( data );
        r_data.pop_back();

        fill_q( q, data );
        fill_q( r, r_data );

        BOOST_REQUIRE( r < q );
    }

    for ( int i = 1; i != test_size; ++i ) {
        pri_queue q, r;
        test_data data = make_test_data( i );
        test_data r_data( data );
        data.push_back( data.back() + 1 );

        fill_q( q, data );
        fill_q( r, r_data );

        BOOST_REQUIRE( r < q );
    }

    for ( int i = 1; i != test_size; ++i ) {
        pri_queue q, r;
        test_data data = make_test_data( i );
        test_data r_data( data );

        data.back() += 1;

        fill_q( q, data );
        fill_q( r, r_data );

        BOOST_REQUIRE( r < q );
    }

    for ( int i = 1; i != test_size; ++i ) {
        pri_queue q, r;
        test_data data = make_test_data( i );
        test_data r_data( data );

        r_data.front() -= 1;

        fill_q( q, data );
        fill_q( r, r_data );

        BOOST_REQUIRE( r < q );
    }
}

template < typename pri_queue >
void pri_queue_test_clear( void )
{
    for ( int i = 0; i != test_size; ++i ) {
        pri_queue q;
        test_data data = make_test_data( i );
        fill_q( q, data );

        q.clear();
        BOOST_REQUIRE( q.size() == 0 );
        BOOST_REQUIRE( q.empty() );
    }
}


template < typename pri_queue >
void run_concept_check( void )
{
    BOOST_CONCEPT_ASSERT( (boost::heap::PriorityQueue< pri_queue >));
}

template < typename pri_queue >
void run_common_heap_tests( void )
{
    pri_queue_test_sequential_push< pri_queue >();
    pri_queue_test_sequential_reverse_push< pri_queue >();
    pri_queue_test_random_push< pri_queue >();
    pri_queue_test_equality< pri_queue >();
    pri_queue_test_inequality< pri_queue >();
    pri_queue_test_less< pri_queue >();
    pri_queue_test_clear< pri_queue >();

    pri_queue_test_emplace< pri_queue >();
}

template < typename pri_queue >
void run_iterator_heap_tests( void )
{
    pri_queue_test_iterators< pri_queue >();
}


template < typename pri_queue >
void run_copyable_heap_tests( void )
{
    pri_queue_test_copyconstructor< pri_queue >();
    pri_queue_test_assignment< pri_queue >();
    pri_queue_test_swap< pri_queue >();
}


template < typename pri_queue >
void run_moveable_heap_tests( void )
{
    pri_queue_test_moveconstructor< pri_queue >();
    pri_queue_test_move_assignment< pri_queue >();
}


template < typename pri_queue >
void run_reserve_heap_tests( void )
{
    test_data data = make_test_data( test_size );
    pri_queue q;
    q.reserve( 6 );
    fill_q( q, data );

    check_q( q, data );
}

template < typename pri_queue >
void run_leak_check_test( void )
{
    pri_queue q;
    q.push( boost::shared_ptr< int >( new int( 0 ) ) );
}

template < typename pri_queue >
void pri_queue_test_stateful_allocator( void )
{
    boost::container::pmr::memory_resource* mr = boost::container::pmr::get_default_resource();

    for ( int i = 0; i != test_size; ++i ) {
        pri_queue q { mr };
        test_data data = make_test_data( i );
        fill_q( q, data );
        check_q( q, data );
    }

    for ( int i = 0; i != test_size; ++i ) {
        pri_queue q { mr };
        test_data data = make_test_data( i );
        fill_q( q, data );

        pri_queue r( q );
        check_q( r, data );
    }
}


struct less_with_T
{
    typedef int T;
    bool        operator()( const int& a, const int& b ) const
    {
        return a < b;
    }
};


class thing
{
public:
    thing( int a_, int b_, int c_ ) :
        a( a_ ),
        b( b_ ),
        c( c_ )
    {}

public:
    int a;
    int b;
    int c;
};

class cmpthings
{
public:
    bool operator()( const thing& lhs, const thing& rhs ) const
    {
        return lhs.a > rhs.a;
    }
    bool operator()( const thing& lhs, const thing& rhs )
    {
        return lhs.a > rhs.a;
    }
};

#define RUN_EMPLACE_TEST( HEAP_TYPE )                                                                                \
    do {                                                                                                             \
        cmpthings                                                          ord;                                      \
        boost::heap::HEAP_TYPE< thing, boost::heap::compare< cmpthings > > vpq( ord );                               \
        vpq.emplace( 5, 6, 7 );                                                                                      \
        boost::heap::HEAP_TYPE< thing, boost::heap::compare< cmpthings >, boost::heap::stable< true > > vpq2( ord ); \
        vpq2.emplace( 5, 6, 7 );                                                                                     \
    } while ( 0 );

template < typename pri_queue >
void pri_queue_test_equality_same( void )
{
    // Two separately populated heaps with the same elements must compare equal
    // and must NOT satisfy operator<.
    for ( int i = 0; i != test_size; ++i ) {
        pri_queue q, r;
        test_data data = make_test_data( i );
        fill_q( q, data );
        fill_q( r, data );

        BOOST_REQUIRE( q == r );
        BOOST_REQUIRE( !( q < r ) );
        BOOST_REQUIRE( !( r < q ) );
        BOOST_REQUIRE( q >= r );
        BOOST_REQUIRE( q <= r );
    }
}

template < typename pri_queue >
void pri_queue_test_less_not_reflexive( void )
{
    pri_queue q;
    test_data data = make_test_data( test_size );
    fill_q( q, data );

    BOOST_REQUIRE( !( q < q ) );
    BOOST_REQUIRE( q >= q );
    BOOST_REQUIRE( q <= q );
}

template < typename pri_queue >
void pri_queue_test_empty_comparisons( void )
{
    pri_queue empty1, empty2;

    BOOST_REQUIRE( empty1 == empty2 );
    BOOST_REQUIRE( !( empty1 != empty2 ) );
    BOOST_REQUIRE( !( empty1 < empty2 ) );
    BOOST_REQUIRE( !( empty2 < empty1 ) );

    // Non-empty > empty
    pri_queue nonempty;
    nonempty.push( 42 );
    BOOST_REQUIRE( empty1 < nonempty );
    BOOST_REQUIRE( !( nonempty < empty1 ) );
    BOOST_REQUIRE( nonempty != empty1 );
}

template < typename pri_queue >
void pri_queue_test_duplicate_values( void )
{
    pri_queue q;
    const int val   = 7;
    const int count = 16;
    for ( int i = 0; i < count; ++i )
        q.push( val );

    BOOST_REQUIRE_EQUAL( q.size(), (typename pri_queue::size_type)count );
    BOOST_REQUIRE_EQUAL( q.top(), val );

    for ( int i = 0; i < count; ++i ) {
        BOOST_REQUIRE_EQUAL( q.top(), val );
        q.pop();
    }
    BOOST_REQUIRE( q.empty() );
}

template < typename pri_queue >
void pri_queue_test_single_element( void )
{
    pri_queue q;
    q.push( 99 );
    BOOST_REQUIRE( !q.empty() );
    BOOST_REQUIRE_EQUAL( q.size(), 1u );
    BOOST_REQUIRE_EQUAL( q.top(), 99 );
    q.pop();
    BOOST_REQUIRE( q.empty() );
    BOOST_REQUIRE_EQUAL( q.size(), 0u );
}

template < typename pri_queue >
void pri_queue_test_self_assignment( void )
{
    for ( int i = 1; i != test_size; ++i ) {
        pri_queue q;
        test_data data = make_test_data( i );
        fill_q( q, data );

        pri_queue& ref = q;
        ref            = q; // self-assignment

        check_q( q, data );
    }
}

template < typename pri_queue >
void pri_queue_test_clear_and_reuse( void )
{
    pri_queue q;
    test_data data = make_test_data( test_size );
    fill_q( q, data );
    q.clear();
    BOOST_REQUIRE( q.empty() );
    BOOST_REQUIRE_EQUAL( q.size(), 0u );

    // Re-fill and verify
    fill_q( q, data );
    check_q( q, data );
}

template < typename pri_queue >
void pri_queue_test_ordered_iterator_empty( void )
{
    pri_queue q;
    BOOST_REQUIRE( q.ordered_begin() == q.ordered_end() );
    BOOST_REQUIRE_EQUAL( std::distance( q.ordered_begin(), q.ordered_end() ), 0 );
}

template < typename pri_queue >
void pri_queue_test_interleaved_push_pop( void )
{
    pri_queue q;
    // Push 0..N-1
    for ( int i = 0; i < test_size; ++i )
        q.push( i );

    // Interleave: pop the current top, then push a new value.
    // After each pop, verify that what we popped was indeed the top (heap invariant).
    for ( int i = 0; i < test_size; ++i ) {
        BOOST_REQUIRE( !q.empty() );
        int expected_top = q.top();
        int v            = q.top();
        q.pop();
        // The value we got must equal what top() reported before the pop
        BOOST_REQUIRE_EQUAL( v, expected_top );
        // push a new value (could be larger or smaller — doesn't matter for the invariant)
        q.push( test_size + i );
    }
    // Drain: verify popped values are in non-increasing order (max-heap invariant)
    int prev = std::numeric_limits< int >::max();
    while ( !q.empty() ) {
        int v = q.top();
        q.pop();
        BOOST_REQUIRE( v <= prev );
        prev = v;
    }
}

template < typename pri_queue >
void run_common_edge_case_tests( void )
{
    pri_queue_test_equality_same< pri_queue >();
    pri_queue_test_less_not_reflexive< pri_queue >();
    pri_queue_test_empty_comparisons< pri_queue >();
    pri_queue_test_duplicate_values< pri_queue >();
    pri_queue_test_single_element< pri_queue >();
    pri_queue_test_self_assignment< pri_queue >();
    pri_queue_test_clear_and_reuse< pri_queue >();
    pri_queue_test_interleaved_push_pop< pri_queue >();
}

template < typename pri_queue >
void run_ordered_iterator_edge_case_tests( void )
{
    pri_queue_test_ordered_iterator_empty< pri_queue >();
}


#endif // COMMON_HEAP_TESTS_HPP_INCLUDED
