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

#elif BOOST_WORKAROUND(BOOST_MSVC, < 1910)

BOOST_PRAGMA_MESSAGE("Test skipped because BOOST_MSVC < 1910")

#elif BOOST_WORKAROUND(BOOST_GCC, < 50000)

BOOST_PRAGMA_MESSAGE("Test skipped because BOOST_GCC < 50000")

#else

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

template<class T> void test()
{
    constexpr boost::array<T, 4> a = {{}};

    STATIC_ASSERT( &a[ 0 ] == a.data() + 0 );
    STATIC_ASSERT( &a[ 1 ] == a.data() + 1 );
    STATIC_ASSERT( &a[ 2 ] == a.data() + 2 );
    STATIC_ASSERT( &a[ 3 ] == a.data() + 3 );

    STATIC_ASSERT( &a.at( 0 ) == a.data() + 0 );
    STATIC_ASSERT( &a.at( 1 ) == a.data() + 1 );
    STATIC_ASSERT( &a.at( 2 ) == a.data() + 2 );
    STATIC_ASSERT( &a.at( 3 ) == a.data() + 3 );

    STATIC_ASSERT( &a.front() == a.data() );
    STATIC_ASSERT( &a.back() == a.data() + 3 );
}

int main()
{
    test<int>();
}

#endif
