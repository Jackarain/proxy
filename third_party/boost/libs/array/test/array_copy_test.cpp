// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/array.hpp>
#include <boost/core/lightweight_test.hpp>
#include <cstddef>

template<class T, std::size_t N> void test1()
{
    boost::array<T, N> a1 = {{}};
    boost::array<T, N> a2 = a1;

    BOOST_TEST_ALL_EQ( a1.begin(), a1.end(), a2.begin(), a2.end() );
}

template<class T, std::size_t N> void test2()
{
    boost::array<T, N> a1 = {};

    boost::array<T, N> a2;
    a2 = a1;

    BOOST_TEST_ALL_EQ( a1.begin(), a1.end(), a2.begin(), a2.end() );
}

template<class T> void test3()
{
    boost::array<T, 4> a1 = {{ 1, 2, 3, 4 }};
    boost::array<T, 4> a2 = a1;

    BOOST_TEST_ALL_EQ( a1.begin(), a1.end(), a2.begin(), a2.end() );
}

template<class T> void test4()
{
    boost::array<T, 4> a1 = { 1, 2, 3, 4 };

    boost::array<T, 4> a2;
    a2 = a1;

    BOOST_TEST_ALL_EQ( a1.begin(), a1.end(), a2.begin(), a2.end() );
}

int main()
{
    test1<int, 0>();
    test1<int, 1>();
    test1<int, 7>();

    test1<int const, 0>();
    test1<int const, 1>();
    test1<int const, 7>();

    test2<int, 0>();
    test2<int, 1>();
    test2<int, 7>();

    test3<int>();
    test3<int const>();

    test4<int>();

    return boost::report_errors();
}
