// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/array.hpp>
#include <boost/core/lightweight_test.hpp>
#include <cstddef>

template<class T, std::size_t N> void test()
{
    {
        boost::array<T, N> a = {{}};

        T* p1 = a.data();

        {
            T* p2 = a.begin();
            T* p3 = a.end();

            BOOST_TEST_EQ( p2, p1 );
            BOOST_TEST_EQ( p3, p1 + N );
        }

        {
            T const* p2 = a.cbegin();
            T const* p3 = a.cend();

            BOOST_TEST_EQ( p2, p1 );
            BOOST_TEST_EQ( p3, p1 + N );
        }
    }

    {
        boost::array<T, N> const a = {{}};

        T const* p1 = a.data();

        {
            T const* p2 = a.begin();
            T const* p3 = a.end();

            BOOST_TEST_EQ( p2, p1 );
            BOOST_TEST_EQ( p3, p1 + N );
        }

        {
            T const* p2 = a.cbegin();
            T const* p3 = a.cend();

            BOOST_TEST_EQ( p2, p1 );
            BOOST_TEST_EQ( p3, p1 + N );
        }
    }
}

template<class T> void test2()
{
    {
        boost::array<T, 0> a = {};

        {
            T* p2 = a.begin();
            T* p3 = a.end();

            BOOST_TEST_EQ( p2, p3 );
        }

        {
            T const* p2 = a.cbegin();
            T const* p3 = a.cend();

            BOOST_TEST_EQ( p2, p3 );
        }
    }

    {
        boost::array<T, 0> const a = {};

        {
            T const* p2 = a.begin();
            T const* p3 = a.end();

            BOOST_TEST_EQ( p2, p3 );
        }

        {
            T const* p2 = a.cbegin();
            T const* p3 = a.cend();

            BOOST_TEST_EQ( p2, p3 );
        }
    }
}

int main()
{
    test<int, 0>();
    test<int, 1>();
    test<int, 7>();

    test<int const, 0>();
    test<int const, 1>();
    test<int const, 7>();

    test2<int>();

    return boost::report_errors();
}
