// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/array.hpp>
#include <boost/config.hpp>
#include <boost/config/pragma_message.hpp>
#include <boost/config/workaround.hpp>
#include <cstddef>

#if defined(BOOST_NO_CXX14_CONSTEXPR)

BOOST_PRAGMA_MESSAGE("Test skipped because BOOST_NO_CXX14_CONSTEXPR is defined")

#else

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

template<class T, std::size_t N> constexpr boost::array<T, N> assigned( boost::array<T, N> const& a1 )
{
    boost::array<T, N> a2 = {};

    a2 = a1;

    return a2;
}

template<class T> void test1()
{
    constexpr boost::array<T, 4> a1 = {{ 1, 2, 3, 4 }};
    constexpr boost::array<T, 4> a2 = assigned( a1 );

    STATIC_ASSERT( a1[0] == a2[0] );
    STATIC_ASSERT( a1[1] == a2[1] );
    STATIC_ASSERT( a1[2] == a2[2] );
    STATIC_ASSERT( a1[3] == a2[3] );
}

template<class T> void test2()
{
    constexpr boost::array<T, 0> a1 = {};
    constexpr boost::array<T, 0> a2 = assigned( a1 );

    (void)a1;
    (void)a2;
}

int main()
{
    test1<int>();
    test2<int>();
}

#endif
