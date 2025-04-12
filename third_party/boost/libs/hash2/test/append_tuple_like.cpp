// Copyright 2017, 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/hash_append.hpp>
#include <boost/hash2/fnv1a.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/array.hpp>
#include <utility>
#include <array>
#include <tuple>

template<class Hash, class Flavor, class T, class R> void test( R r )
{
    Flavor f;

    {
        T v[2] = { 1, 2 };

        Hash h;

        hash_append( h, f, v[0] );
        hash_append( h, f, v[1] );

        BOOST_TEST_EQ( h.result(), r );
    }

    {
        T v[2] = { 1, 2 };

        Hash h;
        hash_append( h, f, v );

        BOOST_TEST_EQ( h.result(), r );
    }

    {
        std::pair<T, T> v( static_cast<T>( 1 ), static_cast<T>( 2 ) );

        Hash h;
        hash_append( h, f, v );

        BOOST_TEST_EQ( h.result(), r );
    }

    {
        boost::array<T, 2> v = {{ 1, 2 }};

        Hash h;
        hash_append( h, f, v );

        BOOST_TEST_EQ( h.result(), r );
    }

    {
        std::array<T, 2> v = {{ 1, 2 }};

        Hash h;
        hash_append( h, f, v );

        BOOST_TEST_EQ( h.result(), r );
    }

    {
        std::tuple<T, T> v( static_cast<T>( 1 ), static_cast<T>( 2 ) );

        Hash h;
        hash_append( h, f, v );

        BOOST_TEST_EQ( h.result(), r );
    }
}

int main()
{
    using namespace boost::hash2;

    test<fnv1a_32, default_flavor, unsigned char>( 3983810698 );
    test<fnv1a_32, little_endian_flavor, unsigned char>( 3983810698 );
    test<fnv1a_32, big_endian_flavor, unsigned char>( 3983810698 );

    test<fnv1a_32, little_endian_flavor, int>( 3738734694 );
    test<fnv1a_32, big_endian_flavor, int>( 1396319868 );

    test<fnv1a_32, little_endian_flavor, double>( 1170904120 );
    test<fnv1a_32, big_endian_flavor, double>( 786481930 );

    return boost::report_errors();
}
