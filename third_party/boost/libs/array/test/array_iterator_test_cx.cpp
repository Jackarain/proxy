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

template<class T, std::size_t N> void test1()
{
    constexpr boost::array<T, N> a = {};

    STATIC_ASSERT( a.begin() == a.data() );
    STATIC_ASSERT( a.end() == a.data() + N );

    STATIC_ASSERT( a.cbegin() == a.data() );
    STATIC_ASSERT( a.cend() == a.data() + N );
}

template<class T> void test2()
{
    constexpr boost::array<T, 0> a = {};

    STATIC_ASSERT( a.begin() == a.end() );
    STATIC_ASSERT( a.cbegin() == a.cend() );
}

int main()
{
    test1<int, 0>();
    test1<int, 1>();
    test1<int, 7>();

    test2<int>();
}

#endif
