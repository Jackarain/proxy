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

template<class T, std::size_t N> constexpr boost::array<T, N> modified( boost::array<T, N> a1 )
{
    a1.front() = 1;
    a1[ 1 ] = 2;
    a1.at( 2 ) = 3;
    a1.back() = 4;

    return a1;
}

template<class T> void test1()
{
    constexpr boost::array<T, 4> a1 = {};
    constexpr boost::array<T, 4> a2 = modified( a1 );

    STATIC_ASSERT( a2[0] == 1 );
    STATIC_ASSERT( a2[1] == 2 );
    STATIC_ASSERT( a2[2] == 3 );
    STATIC_ASSERT( a2[3] == 4 );
}

int main()
{
    test1<int>();
}

#endif
