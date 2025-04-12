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

#else

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

template<class T, std::size_t N> void test1()
{
    constexpr boost::array<T, N> a = {};

    STATIC_ASSERT( a.size() == N );
    STATIC_ASSERT( a.empty() == (N == 0) );
    STATIC_ASSERT( a.max_size() == N );
}

template<class T, std::size_t N> void test2()
{
    typedef boost::array<T, N> A;

    STATIC_ASSERT( A().size() == N );
    STATIC_ASSERT( A().empty() == (N == 0) );
    STATIC_ASSERT( A().max_size() == N );
}

// the functions are static, which deviates from the standard
template<class T, std::size_t N> void test3()
{
    typedef boost::array<T, N> A;

    STATIC_ASSERT( A::size() == N );
    STATIC_ASSERT( A::empty() == (N == 0) );
    STATIC_ASSERT( A::max_size() == N );

    STATIC_ASSERT( A::static_size == N );
}

int main()
{
    test1<int, 0>();
    test1<int, 1>();
    test1<int, 7>();

    test2<int, 0>();
    test2<int, 1>();
    test2<int, 7>();

    test3<int, 0>();
    test3<int, 1>();
    test3<int, 7>();
}

#endif
