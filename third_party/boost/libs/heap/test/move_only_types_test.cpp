/*=============================================================================
    Copyright (c) 2026 Tim Blechmann

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#define BOOST_TEST_MAIN
#include <boost/mpl/list.hpp>
#include <boost/test/unit_test.hpp>

#include <boost/heap/binomial_heap.hpp>
#include <boost/heap/d_ary_heap.hpp>
#include <boost/heap/fibonacci_heap.hpp>
#include <boost/heap/pairing_heap.hpp>
#include <boost/heap/skew_heap.hpp>

// Move-only type that cannot be copied
struct MoveOnlyInt
{
    explicit MoveOnlyInt( int val = 0 ) :
        value( val )
    {}

    MoveOnlyInt( MoveOnlyInt&& other ) noexcept :
        value( other.value )
    {
        other.value = -1; // Mark as moved from
    }

    MoveOnlyInt& operator=( MoveOnlyInt&& other ) noexcept
    {
        value       = other.value;
        other.value = -1;
        return *this;
    }

    // Explicitly delete copy operations
    MoveOnlyInt( MoveOnlyInt const& )            = delete;
    MoveOnlyInt& operator=( MoveOnlyInt const& ) = delete;

    friend bool operator<( MoveOnlyInt const& lhs, MoveOnlyInt const& rhs )
    {
        return lhs.value < rhs.value;
    }

    friend bool operator<=( MoveOnlyInt const& lhs, MoveOnlyInt const& rhs )
    {
        return lhs.value <= rhs.value;
    }

    friend bool operator>( MoveOnlyInt const& lhs, MoveOnlyInt const& rhs )
    {
        return lhs.value > rhs.value;
    }

    friend bool operator>=( MoveOnlyInt const& lhs, MoveOnlyInt const& rhs )
    {
        return lhs.value >= rhs.value;
    }

    friend bool operator==( MoveOnlyInt const& lhs, MoveOnlyInt const& rhs )
    {
        return lhs.value == rhs.value;
    }

    friend bool operator!=( MoveOnlyInt const& lhs, MoveOnlyInt const& rhs )
    {
        return lhs.value != rhs.value;
    }

    int value;
};

template < typename HeapType >
void test_move_only_basic( void )
{
    HeapType pq;

    // Test emplace
    pq.emplace( 5 );
    pq.emplace( 3 );
    pq.emplace( 7 );
    pq.emplace( 1 );

    BOOST_REQUIRE_EQUAL( pq.size(), 4u );
    BOOST_REQUIRE_EQUAL( pq.top().value, 7 );

    pq.pop();
    BOOST_REQUIRE_EQUAL( pq.top().value, 5 );

    pq.pop();
    BOOST_REQUIRE_EQUAL( pq.top().value, 3 );

    pq.pop();
    BOOST_REQUIRE_EQUAL( pq.top().value, 1 );

    pq.pop();
    BOOST_REQUIRE( pq.empty() );
}

template < typename HeapType >
void test_move_only_emplace_temporary( void )
{
    HeapType pq;

    // Test emplace with temporary values that will be move-constructed
    pq.emplace( 42 );
    pq.emplace( 13 );
    pq.emplace( 99 );

    BOOST_REQUIRE_EQUAL( pq.size(), 3u );
    BOOST_REQUIRE_EQUAL( pq.top().value, 99 );
}

template < typename HeapType >
void test_move_only_move_semantics( void )
{
    HeapType pq1;
    pq1.emplace( 10 );
    pq1.emplace( 20 );
    pq1.emplace( 30 );

    HeapType pq2 = std::move( pq1 );

    BOOST_REQUIRE( pq1.empty() );
    BOOST_REQUIRE_EQUAL( pq2.size(), 3u );
    BOOST_REQUIRE_EQUAL( pq2.top().value, 30 );
}

typedef boost::mpl::list< boost::heap::binomial_heap< MoveOnlyInt >,
                          boost::heap::binomial_heap< MoveOnlyInt, boost::heap::stable< true > >,
                          boost::heap::fibonacci_heap< MoveOnlyInt >,
                          boost::heap::fibonacci_heap< MoveOnlyInt, boost::heap::stable< true > >,
                          boost::heap::pairing_heap< MoveOnlyInt >,
                          boost::heap::pairing_heap< MoveOnlyInt, boost::heap::stable< true > >,
                          boost::heap::skew_heap< MoveOnlyInt >,
                          boost::heap::skew_heap< MoveOnlyInt, boost::heap::stable< true > >,
                          boost::heap::d_ary_heap< MoveOnlyInt, boost::heap::arity< 4 > >,
                          boost::heap::d_ary_heap< MoveOnlyInt, boost::heap::arity< 4 >, boost::heap::stable< true > > >
    default_heaps;

BOOST_AUTO_TEST_CASE_TEMPLATE( move_only_basic, Heap, default_heaps )
{
    test_move_only_basic< Heap >();
}

BOOST_AUTO_TEST_CASE_TEMPLATE( move_only_emplace_temporary, Heap, default_heaps )
{
    test_move_only_emplace_temporary< Heap >();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct CustomCompare
{
    bool operator()( MoveOnlyInt const& lhs, MoveOnlyInt const& rhs ) const
    {
        return lhs.value > rhs.value;
    }
};

template < typename HeapType >
void test_move_only_custom_compare( void )
{
    HeapType pq;

    pq.emplace( 5 );
    pq.emplace( 3 );
    pq.emplace( 7 );

    BOOST_REQUIRE_EQUAL( pq.top().value, 3 );
    pq.pop();
    BOOST_REQUIRE_EQUAL( pq.top().value, 5 );
}

// Heaps with custom comparator
typedef boost::mpl::list<
    boost::heap::binomial_heap< MoveOnlyInt, boost::heap::compare< CustomCompare > >,
    boost::heap::fibonacci_heap< MoveOnlyInt, boost::heap::compare< CustomCompare > >,
    boost::heap::pairing_heap< MoveOnlyInt, boost::heap::compare< CustomCompare > >,
    boost::heap::skew_heap< MoveOnlyInt, boost::heap::compare< CustomCompare > >,
    boost::heap::d_ary_heap< MoveOnlyInt, boost::heap::arity< 4 >, boost::heap::compare< CustomCompare > > >
    custom_heaps;

BOOST_AUTO_TEST_CASE_TEMPLATE( move_only_move_semantics, Heap, default_heaps )
{
    test_move_only_move_semantics< Heap >();
}

BOOST_AUTO_TEST_CASE_TEMPLATE( move_only_custom_compare, Heap, custom_heaps )
{
    test_move_only_custom_compare< Heap >();
}
