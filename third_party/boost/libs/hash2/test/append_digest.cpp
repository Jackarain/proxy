// Copyright 2017, 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/hash_append.hpp>
#include <boost/hash2/fnv1a.hpp>
#include <boost/hash2/digest.hpp>
#include <boost/core/lightweight_test.hpp>
#include <array>

template<class Hash, class Flavor, class R> void test( R r )
{
    Flavor f;

    {
        unsigned char v[4] = { 1, 2, 3, 4 };

        Hash h;

        hash_append( h, f, v );

        BOOST_TEST_EQ( h.result(), r );
    }

    {
        std::array<unsigned char, 4> v = {{ 1, 2, 3, 4 }};

        Hash h;

        hash_append( h, f, v );

        BOOST_TEST_EQ( h.result(), r );
    }

    {
        boost::hash2::digest<4> v = {{ 1, 2, 3, 4 }};

        Hash h;

        hash_append( h, f, v );

        BOOST_TEST_EQ( h.result(), r );
    }
}

int main()
{
    using namespace boost::hash2;

    test<fnv1a_32, default_flavor>( 1463068797 );
    test<fnv1a_32, little_endian_flavor>( 1463068797 );
    test<fnv1a_32, big_endian_flavor>( 1463068797 );

    return boost::report_errors();
}
