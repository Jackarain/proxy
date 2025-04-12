// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/array.hpp>
#include <boost/core/lightweight_test.hpp>
#include <cstddef>

#if defined(_MSC_VER)
# pragma warning(disable: 4702) // unreachable code
#endif

template<class T, std::size_t N> void test()
{
    {
        boost::array<T, N> const a1 = {};
        boost::array<T, N> const a2 = {};

        BOOST_TEST( a1 == a2 );
        BOOST_TEST_NOT( a1 != a2 );
    }

    for( std::size_t i = 0; i < N; ++i )
    {
        boost::array<T, N> const a1 = {};

        boost::array<T, N> a2 = a1;
        a2[ i ] = 1;

        BOOST_TEST( a1 != a2 );
        BOOST_TEST_NOT( a1 == a2 );

        boost::array<T, N> const a3 = a2;

        BOOST_TEST( a1 != a3 );
        BOOST_TEST_NOT( a1 == a3 );
    }

    {
        boost::array<T, N> a1 = {};
        boost::array<T, N> a2 = {};

        a1.fill( 1 );
        a2.fill( 1 );

        BOOST_TEST( a1 == a2 );
        BOOST_TEST_NOT( a1 != a2 );
    }
}

template<class T> void test2()
{
    {
        boost::array<T, 4> const a1 = {{ 1, 2, 3, 4 }};
        boost::array<T, 4> const a2 = {{ 1, 2, 3, 4 }};

        BOOST_TEST( a1 == a2 );
        BOOST_TEST_NOT( a1 != a2 );
    }

    for( std::size_t i = 0; i < 4; ++i )
    {
        boost::array<T, 4> const a1 = {{ 1, 2, 3, 4 }};

        boost::array<T, 4> a2 = a1;
        a2[ i ] = 0;

        BOOST_TEST( a1 != a2 );
        BOOST_TEST_NOT( a1 == a2 );

        boost::array<T, 4> const a3 = a2;

        BOOST_TEST( a1 != a3 );
        BOOST_TEST_NOT( a1 == a3 );
    }
}

int main()
{
    test<int, 0>();
    test<int, 1>();
    test<int, 7>();

    test2<int>();

    return boost::report_errors();
}
