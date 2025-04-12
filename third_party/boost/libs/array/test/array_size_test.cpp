// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/array.hpp>
#include <boost/core/lightweight_test.hpp>
#include <iterator>
#include <cstddef>

template<class T, std::size_t N> void test1()
{
    {
        boost::array<T, N> a = {{}};

        BOOST_TEST_EQ( a.size(), N );
        BOOST_TEST_EQ( a.empty(), N == 0 );
        BOOST_TEST_EQ( a.max_size(), N );

        (void)a; // msvc-12.0
    }

    {
        boost::array<T, N> const a = {{}};

        BOOST_TEST_EQ( a.size(), N );
        BOOST_TEST_EQ( a.empty(), N == 0 );
        BOOST_TEST_EQ( a.max_size(), N );

        (void)a; // msvc-12.0
    }
}

template<class T, std::size_t N> void test2()
{
    typedef boost::array<T, N> A;

    BOOST_TEST_EQ( A().size(), N );
    BOOST_TEST_EQ( A().empty(), N == 0 );
    BOOST_TEST_EQ( A().max_size(), N );
}

// the functions are static, which deviates from the standard
template<class T, std::size_t N> void test3()
{
    typedef boost::array<T, N> A;

    BOOST_TEST_EQ( A::size(), N );
    BOOST_TEST_EQ( A::empty(), N == 0 );
    BOOST_TEST_EQ( A::max_size(), N );

    BOOST_TEST_EQ( A::static_size, N );
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

    test3<int, 0>();
    test3<int, 1>();
    test3<int, 7>();

    test3<int const, 0>();
    test3<int const, 1>();
    test3<int const, 7>();

    return boost::report_errors();
}
