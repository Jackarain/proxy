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

template<class T, std::size_t N> void test1()
{
    constexpr boost::array<T, N> a1 = {};
    constexpr boost::array<T, N> a2 = {};

    STATIC_ASSERT( !( a1 < a2 ) );
    STATIC_ASSERT( !( a1 > a2 ) );

    STATIC_ASSERT( a1 <= a2 );
    STATIC_ASSERT( a1 >= a2 );
}

template<class T> void test2()
{
    {
        constexpr boost::array<T, 4> a1 = {{ 1, 2, 3, 4 }};
        constexpr boost::array<T, 4> a2 = {{ 1, 2, 3, 4 }};

        STATIC_ASSERT( !( a1 < a2 ) );
        STATIC_ASSERT( !( a1 > a2 ) );

        STATIC_ASSERT( a1 <= a2 );
        STATIC_ASSERT( a1 >= a2 );
    }
    {
        constexpr boost::array<T, 4> a1 = {{ 1, 2, 3, 4 }};
        constexpr boost::array<T, 4> a2 = {{ 1, 2, 3, 5 }};

        STATIC_ASSERT( a1 < a2 );
        STATIC_ASSERT( !( a1 > a2 ) );

        STATIC_ASSERT( a1 <= a2 );
        STATIC_ASSERT( !( a1 >= a2 ) );
    }
    {
        constexpr boost::array<T, 4> a1 = {{ 1, 2, 3, 4 }};
        constexpr boost::array<T, 4> a2 = {{ 1, 2, 3, 2 }};

        STATIC_ASSERT( !( a1 < a2 ) );
        STATIC_ASSERT( a1 > a2 );

        STATIC_ASSERT( !( a1 <= a2 ) );
        STATIC_ASSERT( a1 >= a2 );
    }
}

int main()
{
    test1<int, 0>();
    test1<int, 1>();
    test1<int, 7>();

    test2<int>();
}

#endif
