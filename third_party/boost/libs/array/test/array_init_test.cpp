// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/array.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <boost/config/workaround.hpp>
#include <cstddef>

#if defined(_MSC_VER)
# pragma warning(disable: 4702) // unreachable code
#endif

template<class T, std::size_t N> void test1()
{
    boost::array<T, N> a = {{}};

    for( std::size_t i = 0; i < N; ++i )
    {
        BOOST_TEST_EQ( a[i], T() );
    }
}

template<class T, std::size_t N> void test2()
{
    boost::array<T, N> a = {};

    for( std::size_t i = 0; i < N; ++i )
    {
        BOOST_TEST_EQ( a[i], T() );
    }
}

template<class T> void test3()
{
    boost::array<T, 4> a = {{ 1, 2, 3, 4 }};
    T b[ 4 ] = { 1, 2, 3, 4 };

    BOOST_TEST_ALL_EQ( a.begin(), a.end(), b + 0, b + 4 );
}

template<class T> void test4()
{
    boost::array<T, 4> a = { 1, 2, 3, 4 };
    T b[ 4 ] = { 1, 2, 3, 4 };

    BOOST_TEST_ALL_EQ( a.begin(), a.end(), b + 0, b + 4 );
}

int main()
{
    test1<int, 0>();
    test1<int, 1>();
    test1<int, 7>();

    test1<int const, 0>();
    test1<int const, 1>();
    test1<int const, 7>();

    test2<int, 0>();
    test2<int, 1>();
    test2<int, 7>();

    test2<int const, 0>();

#if BOOST_WORKAROUND(BOOST_MSVC, < 1910) || BOOST_WORKAROUND(BOOST_GCC, < 50000)

    // = {} doesn't work for const T

#else

    test2<int const, 1>();
    test2<int const, 7>();

#endif

    test3<int>();
    test3<int const>();

    test4<int>();
    test4<int const>();

    return boost::report_errors();
}
