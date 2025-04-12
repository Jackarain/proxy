// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#define BOOST_ENABLE_ASSERT_HANDLER

#include <boost/array.hpp>
#include <boost/core/lightweight_test.hpp>
#include <cstddef>

struct assertion_failure
{
};

namespace boost
{

void assertion_failed( char const* /*expr*/, char const* /*function*/, char const* /*file*/, long /*line*/ )
{
    throw assertion_failure();
}

void assertion_failed_msg( char const* /*expr*/, char const* /*msg*/, char const* /*function*/, char const* /*file*/, long /*line*/ )
{
    throw assertion_failure();
}

} // namespace boost

template<class T, std::size_t N> void test()
{
    {
        boost::array<T, N> a = {{}};

        T* p1 = a.data();

        for( std::size_t i = 0; i < N; ++i )
        {
            T* p2 = &a[ i ];
            T* p3 = &a.at( i );

            BOOST_TEST_EQ( p2, p1 + i );
            BOOST_TEST_EQ( p3, p1 + i );
        }

        {
            T* p2 = &a.front();
            T* p3 = &a.back();

            BOOST_TEST_EQ( p2, p1 + 0 );
            BOOST_TEST_EQ( p3, p1 + N - 1 );
        }

        BOOST_TEST_THROWS( a.at( N ), std::out_of_range );
        BOOST_TEST_THROWS( a[ N ], assertion_failure );
    }

    {
        boost::array<T, N> const a = {{}};

        T const* p1 = a.data();

        for( std::size_t i = 0; i < N; ++i )
        {
            T const* p2 = &a[ i ];
            T const* p3 = &a.at( i );

            BOOST_TEST_EQ( p2, p1 + i );
            BOOST_TEST_EQ( p3, p1 + i );
        }

        {
            T const* p2 = &a.front();
            T const* p3 = &a.back();

            BOOST_TEST_EQ( p2, p1 + 0 );
            BOOST_TEST_EQ( p3, p1 + N - 1 );
        }

        BOOST_TEST_THROWS( a.at( N ), std::out_of_range );
        BOOST_TEST_THROWS( a[ N ], assertion_failure );
    }
}

template<class T> void test2()
{
    {
        boost::array<T, 0> a = {};

        BOOST_TEST_THROWS( a.at( 0 ), std::out_of_range );
        BOOST_TEST_THROWS( a[ 0 ], std::out_of_range );
        BOOST_TEST_THROWS( a.front(), std::out_of_range );
        BOOST_TEST_THROWS( a.back(), std::out_of_range );
    }

    {
        boost::array<T, 0> const a = {};

        BOOST_TEST_THROWS( a.at( 0 ), std::out_of_range );
        BOOST_TEST_THROWS( a[ 0 ], std::out_of_range );
        BOOST_TEST_THROWS( a.front(), std::out_of_range );
        BOOST_TEST_THROWS( a.back(), std::out_of_range );
    }
}

int main()
{
    test<int, 1>();
    test<int, 7>();

    test<int const, 1>();
    test<int const, 7>();

    test2<int>();

    return boost::report_errors();
}
