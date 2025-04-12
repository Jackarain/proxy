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
    constexpr boost::array<T, 1> a = {{}};

    STATIC_ASSERT( a[0] == 0 );
}

template<class T> void test2()
{
    constexpr boost::array<T, 2> a = {};

    STATIC_ASSERT( a[0] == 0 );
    STATIC_ASSERT( a[1] == 0 );
}

template<class T> void test3()
{
    constexpr boost::array<T, 3> a = {{ 1, 2, 3 }};

    STATIC_ASSERT( a[0] == 1 );
    STATIC_ASSERT( a[1] == 2 );
    STATIC_ASSERT( a[2] == 3 );
}

template<class T> void test4()
{
    constexpr boost::array<T, 4> a = { 1, 2, 3, 4 };

    STATIC_ASSERT( a[0] == 1 );
    STATIC_ASSERT( a[1] == 2 );
    STATIC_ASSERT( a[2] == 3 );
    STATIC_ASSERT( a[3] == 4 );
}

template<class T> void test5()
{
    constexpr boost::array<T, 0> a = {{}};
    (void)a;
}

template<class T> void test6()
{
    constexpr boost::array<T, 0> a = {};
    (void)a;
}

int main()
{
    test1<int>();
    test2<int>();
    test3<int>();
    test4<int>();
    test5<int>();
    test6<int>();
}

#endif
