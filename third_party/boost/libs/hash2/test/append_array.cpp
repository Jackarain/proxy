// Copyright 2017, 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/hash_append.hpp>
#include <boost/hash2/fnv1a.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/array.hpp>
#include <array>

template<class Hash, class Flavor, class T, class R> void test( R r )
{
    Flavor f;

    {
        T v[4] = { 1, 2, 3, 4 };

        Hash h;

        hash_append( h, f, v[0] );
        hash_append( h, f, v[1] );
        hash_append( h, f, v[2] );
        hash_append( h, f, v[3] );

        BOOST_TEST_EQ( h.result(), r );
    }

    {
        T v[4] = { 1, 2, 3, 4 };

        Hash h;

        hash_append( h, f, v );

        BOOST_TEST_EQ( h.result(), r );
    }

    {
        T v[2][2] = { { 1, 2 }, { 3, 4 } };

        Hash h;

        hash_append( h, f, v );

        BOOST_TEST_EQ( h.result(), r );
    }

    {
        boost::array<T, 4> v = {{ 1, 2, 3, 4 }};

        Hash h;

        hash_append( h, f, v );

        BOOST_TEST_EQ( h.result(), r );
    }

    {
        std::array<T, 4> v = {{ 1, 2, 3, 4 }};

        Hash h;

        hash_append( h, f, v );

        BOOST_TEST_EQ( h.result(), r );
    }
}

int main()
{
    using namespace boost::hash2;

    test<fnv1a_32, default_flavor, unsigned char>( 1463068797 );
    test<fnv1a_32, little_endian_flavor, unsigned char>( 1463068797 );
    test<fnv1a_32, big_endian_flavor, unsigned char>( 1463068797 );

    test<fnv1a_32, little_endian_flavor, int>( 1041505217 );
    test<fnv1a_32, big_endian_flavor, int>( 3216558841 );

    test<fnv1a_32, little_endian_flavor, double>( 4043691488 );
    test<fnv1a_32, big_endian_flavor, double>( 3684964226 );

    return boost::report_errors();
}
