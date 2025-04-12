// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/array.hpp>
#include <boost/config.hpp>
#include <boost/config/pragma_message.hpp>
#include <boost/config/workaround.hpp>
#include <cstddef>

#if defined(BOOST_NO_CXX11_CONSTEXPR)

BOOST_PRAGMA_MESSAGE("Test skipped because BOOST_NO_CXX11_CONSTEXPR is defined")

#elif BOOST_WORKAROUND(BOOST_GCC, < 50000)

BOOST_PRAGMA_MESSAGE("Test skipped because BOOST_GCC < 50000")

#else

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

template<class T> void test1()
{
    constexpr boost::array<T, 2> a1 = {};
    constexpr boost::array<T, 2> a2 = a1;

    STATIC_ASSERT( a1[0] == a2[0] );
    STATIC_ASSERT( a1[1] == a2[1] );
}

template<class T> void test2()
{
    constexpr boost::array<T, 3> a1 = {{ 1, 2, 3 }};
    constexpr boost::array<T, 3> a2 = a1;

    STATIC_ASSERT( a1[0] == a2[0] );
    STATIC_ASSERT( a1[1] == a2[1] );
    STATIC_ASSERT( a1[2] == a2[2] );
}

template<class T> void test3()
{
    constexpr boost::array<T, 0> a1 = {};
    constexpr boost::array<T, 0> a2 = a1;

    (void)a1;
    (void)a2;
}

int main()
{
    test1<int>();
    test2<int>();
    test3<int>();
}

#endif
