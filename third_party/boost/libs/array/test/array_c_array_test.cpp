// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#define BOOST_ALLOW_DEPRECATED_SYMBOLS

#include <boost/array.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <boost/config/workaround.hpp>
#include <cstddef>

// c_array and get_c_array are deprecated nonstandard extensions

template<class T, std::size_t N> void test()
{
    boost::array<T, N> a = {};

    T* p1 = a.c_array();
    T* p2 = a.data();

    BOOST_TEST_EQ( p1, p2 );
}

template<class T, std::size_t N> void test2()
{
    {
        boost::array<T, N> a = {{}};

        T (&e1)[ N ] = boost::get_c_array( a );
        T (&e2)[ N ] = a.elems;

        BOOST_TEST_EQ( static_cast<T*>( e1 ), static_cast<T*>( e2 ) );
    }
    {
        boost::array<T, N> const a = {{}};

        T const (&e1)[ N ] = boost::get_c_array( a );
        T const (&e2)[ N ] = a.elems;

        BOOST_TEST_EQ( static_cast<T const*>( e1 ), static_cast<T const*>( e2 ) );
    }
}

int main()
{
    test<int, 0>();
    test<int, 1>();
    test<int, 7>();

    test<int const, 0>();

#if BOOST_WORKAROUND(BOOST_MSVC, < 1910) || BOOST_WORKAROUND(BOOST_GCC, < 50000)

    // = {} doesn't work for const T

#else

    test<int const, 1>();
    test<int const, 7>();

#endif

    test2<int, 1>();
    test2<int, 7>();

    test2<int const, 1>();
    test2<int const, 7>();

    return boost::report_errors();
}
