// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/smart_ptr/detail/quick_allocator.hpp>
#include <boost/core/lightweight_test.hpp>
#include <cstring>

// The interface of quick_allocator has never been documented,
// but in can be inferred from the source code to be
//
// template<class T> struct quick_allocator
// {
//     // allocate memory for an object of type T
//     static void* alloc();
//
//     // deallocate the memory returned from alloc()
//     // if p == 0, no effect
//     static void dealloc( void* p );
//
//     // if n == sizeof(T), returns alloc()
//     // otherwise, allocates a memory block of size n
//     static void* alloc( std::size_t n );
//
//     // deallocate the memory returned from alloc( n )
//     // if p == 0, no effect
//     static void dealloc( void* p, std::size_t n );
//  };

struct X
{
    int data;
};

struct Y: public X
{
    int data2;
};

int main()
{
    using boost::detail::quick_allocator;

    void* p = quick_allocator<Y>::alloc();
    std::memset( p, 0xAA, sizeof(Y) );

    {
        void* p1 = quick_allocator<X>::alloc();
        std::memset( p1, 0xCC, sizeof(X) );

        void* p2 = quick_allocator<X>::alloc();
        std::memset( p2, 0xDD, sizeof(X) );

        BOOST_TEST_NE( p1, p2 );
        BOOST_TEST_EQ( *static_cast<unsigned char*>( p1 ), 0xCC );
        BOOST_TEST_EQ( *static_cast<unsigned char*>( p2 ), 0xDD );

        quick_allocator<X>::dealloc( 0 );
        BOOST_TEST_EQ( *static_cast<unsigned char*>( p1 ), 0xCC );
        BOOST_TEST_EQ( *static_cast<unsigned char*>( p2 ), 0xDD );

        quick_allocator<X>::dealloc( p1 );
        BOOST_TEST_EQ( *static_cast<unsigned char*>( p2 ), 0xDD );

        quick_allocator<X>::dealloc( p2 );
    }

    {
        void* p1 = quick_allocator<X>::alloc( sizeof(X) );
        std::memset( p1, 0xCC, sizeof(X) );

        void* p2 = quick_allocator<X>::alloc( sizeof(Y) );
        std::memset( p2, 0xDD, sizeof(Y) );

        BOOST_TEST_NE( p1, p2 );
        BOOST_TEST_EQ( *static_cast<unsigned char*>( p1 ), 0xCC );
        BOOST_TEST_EQ( *static_cast<unsigned char*>( p2 ), 0xDD );

        quick_allocator<X>::dealloc( 0, sizeof(X) );
        BOOST_TEST_EQ( *static_cast<unsigned char*>( p1 ), 0xCC );
        BOOST_TEST_EQ( *static_cast<unsigned char*>( p2 ), 0xDD );

        quick_allocator<X>::dealloc( p1, sizeof(X) );
        BOOST_TEST_EQ( *static_cast<unsigned char*>( p2 ), 0xDD );

        quick_allocator<X>::dealloc( 0, sizeof(Y) );
        BOOST_TEST_EQ( *static_cast<unsigned char*>( p2 ), 0xDD );

        quick_allocator<X>::dealloc( p2, sizeof(Y) );
    }

    BOOST_TEST_EQ( *static_cast<unsigned char*>( p ), 0xAA );
    quick_allocator<Y>::dealloc( 0 );

    BOOST_TEST_EQ( *static_cast<unsigned char*>( p ), 0xAA );
    quick_allocator<Y>::dealloc( p );

    return boost::report_errors();
}
