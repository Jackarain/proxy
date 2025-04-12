// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#define BOOST_ALLOW_DEPRECATED_SYMBOLS

// assign is a deprecated nonstandard equivalent of fill

#include <boost/array.hpp>
#include <boost/core/lightweight_test.hpp>
#include <cstddef>

#if defined(_MSC_VER)
# pragma warning(disable: 4702) // unreachable code
#endif

template<class T, std::size_t N> void test()
{
    boost::array<T, N> a = {};

    a.assign( 1 );

    for( std::size_t i = 0; i < N; ++i )
    {
        BOOST_TEST_EQ( a[i], 1 );
    }
}

template<class T> void test2()
{
    boost::array<T, 4> a = {{ 1, 2, 3, 4 }};

    a.assign( 5 );

    for( std::size_t i = 0; i < 4; ++i )
    {
        BOOST_TEST_EQ( a[i], 5 );
    }
}

template<class T> void test3()
{
    boost::array<T, 4> a = {{ 1, 2, 3, 4 }};

    // aliasing
    a.assign( a[ 1 ] );

    for( std::size_t i = 0; i < 4; ++i )
    {
        BOOST_TEST_EQ( a[i], 2 );
    }
}

int main()
{
    test<int, 0>();
    test<int, 1>();
    test<int, 7>();

    test2<int>();

    test3<int>();

    return boost::report_errors();
}
