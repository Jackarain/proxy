// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/array.hpp>
#include <boost/core/lightweight_test.hpp>
#include <cstddef>

template<class T, class U, std::size_t N> void test2()
{
    boost::array<T, N> a1 = {};

    boost::array<U, N> a2;
    a2 = a1;

    BOOST_TEST_ALL_EQ( a1.begin(), a1.end(), a2.begin(), a2.end() );
}

template<class T, class U> void test4()
{
    boost::array<T, 4> a1 = { 1, 2, 3, 4 };

    boost::array<U, 4> a2;
    a2 = a1;

    BOOST_TEST_ALL_EQ( a1.begin(), a1.end(), a2.begin(), a2.end() );
}

int main()
{
    test2<int, long, 0>();
    test2<int, long, 1>();
    test2<int, long, 7>();

    test4<int, long>();

    return boost::report_errors();
}
