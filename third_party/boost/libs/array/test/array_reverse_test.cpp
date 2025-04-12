// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/array.hpp>
#include <boost/core/lightweight_test.hpp>
#include <iterator>
#include <cstddef>

template<class T, std::size_t N> void test()
{
    {
        boost::array<T, N> a = {{}};

        {
            std::reverse_iterator<T*> r1( a.rbegin() );
            std::reverse_iterator<T*> r2( a.end() );

            BOOST_TEST( r1 == r2 );
        }

        {
            std::reverse_iterator<T*> r1( a.rend() );
            std::reverse_iterator<T*> r2( a.begin() );

            BOOST_TEST( r1 == r2 );
        }

        {
            std::reverse_iterator<T const*> r1( a.crbegin() );
            std::reverse_iterator<T const*> r2( a.cend() );

            BOOST_TEST( r1 == r2 );
        }

        {
            std::reverse_iterator<T const*> r1( a.crend() );
            std::reverse_iterator<T const*> r2( a.cbegin() );

            BOOST_TEST( r1 == r2 );
        }
    }

    {
        boost::array<T, N> const a = {{}};

        {
            std::reverse_iterator<T const*> r1( a.rbegin() );
            std::reverse_iterator<T const*> r2( a.end() );

            BOOST_TEST( r1 == r2 );
        }

        {
            std::reverse_iterator<T const*> r1( a.rend() );
            std::reverse_iterator<T const*> r2( a.begin() );

            BOOST_TEST( r1 == r2 );
        }

        {
            std::reverse_iterator<T const*> r1( a.crbegin() );
            std::reverse_iterator<T const*> r2( a.cend() );

            BOOST_TEST( r1 == r2 );
        }

        {
            std::reverse_iterator<T const*> r1( a.crend() );
            std::reverse_iterator<T const*> r2( a.cbegin() );

            BOOST_TEST( r1 == r2 );
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

    return boost::report_errors();
}
