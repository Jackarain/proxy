// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/array.hpp>
#include <boost/core/lightweight_test.hpp>
#include <cstddef>

template<class T> void test()
{
    {
        boost::array<T, 4> const a = {{ 1, 2, 3, 4 }};

        BOOST_TEST_EQ( boost::get<0>( a ), 1 );
        BOOST_TEST_EQ( boost::get<1>( a ), 2 );
        BOOST_TEST_EQ( boost::get<2>( a ), 3 );
        BOOST_TEST_EQ( boost::get<3>( a ), 4 );
    }

    {
        boost::array<T, 4> a = {{ 1, 2, 3, 4 }};

        boost::get<0>( a ) += 1;
        boost::get<1>( a ) += 2;
        boost::get<2>( a ) += 3;
        boost::get<3>( a ) += 4;

        BOOST_TEST_EQ( boost::get<0>( a ), 2 );
        BOOST_TEST_EQ( boost::get<1>( a ), 4 );
        BOOST_TEST_EQ( boost::get<2>( a ), 6 );
        BOOST_TEST_EQ( boost::get<3>( a ), 8 );
    }
}

int main()
{
    test<int>();

    return boost::report_errors();
}
