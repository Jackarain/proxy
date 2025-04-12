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

template<class T, std::size_t N> constexpr boost::array<T, N> filled( T const& v )
{
    boost::array<T, N> r = {};

    r.fill( v );

    return r;
}

template<class T> void test1()
{
    constexpr boost::array<T, 4> a1 = filled<T, 4>( 7 );

    STATIC_ASSERT( a1[0] == 7 );
    STATIC_ASSERT( a1[1] == 7 );
    STATIC_ASSERT( a1[2] == 7 );
    STATIC_ASSERT( a1[3] == 7 );
}

template<class T> void test2()
{
    constexpr boost::array<T, 0> a1 = filled<T, 0>( 7 );

    (void)a1;
}

int main()
{
    test1<int>();
    test2<int>();
}

#endif
