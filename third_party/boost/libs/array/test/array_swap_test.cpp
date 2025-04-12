// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/array.hpp>
#include <boost/core/lightweight_test.hpp>
#include <cstddef>

#if defined(_MSC_VER)
# pragma warning(disable: 4702) // unreachable code
#endif

template<class T, std::size_t N> void test()
{
    boost::array<T, N> a1 = {};
    boost::array<T, N> a2 = {};

    a1.fill( 1 );
    a2.fill( 2 );

    a1.swap( a2 );

    for( std::size_t i = 0; i < N; ++i )
    {
        BOOST_TEST_EQ( a1[i], 2 );
        BOOST_TEST_EQ( a2[i], 1 );
    }
}

template<class T> void test2()
{
    boost::array<T, 4> a1 = {{ 1, 2, 3, 4 }};
    boost::array<T, 4> a2 = {{ 5, 6, 7, 8 }};

    a1.swap( a2 );

    for( int i = 0; i < 4; ++i )
    {
        BOOST_TEST_EQ( a1[i], i+5 );
        BOOST_TEST_EQ( a2[i], i+1 );
    }
}

int main()
{
    test<int, 0>();
    test<int, 1>();
    test<int, 7>();

    test2<int>();

    return boost::report_errors();
}
