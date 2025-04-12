// Copyright 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/hash_append.hpp>
#include <boost/hash2/fnv1a.hpp>
#include <boost/core/lightweight_test.hpp>
#include <limits>
#include <cstdint>

template<class Hash, class Flavor, class T> void test( T const& v, unsigned char const (&ref)[ sizeof(T) ] )
{
    {
        typename Hash::result_type r1, r2;

        {
            Hash h;
            Flavor f;

            hash_append( h, f, v );

            r1 = h.result();
        }

        {
            Hash h;

            h.update( ref, sizeof(ref) );

            r2 = h.result();
        }

        BOOST_TEST_EQ( r1, r2 );
    }
}

int main()
{
    using namespace boost::hash2;

    // float

    test<fnv1a_32, default_flavor>( +0.0f, { 0x00, 0x00, 0x00, 0x00 } );
    test<fnv1a_32, default_flavor>( -0.0f, { 0x00, 0x00, 0x00, 0x00 } );

    test<fnv1a_32, big_endian_flavor>( +1.0f, { 0x3F, 0x80, 0x00, 0x00 } );
    test<fnv1a_32, big_endian_flavor>( -1.0f, { 0xBF, 0x80, 0x00, 0x00 } );

    test<fnv1a_32, big_endian_flavor>( +3.14f, { 0x40, 0x48, 0xF5, 0xC3 } );
    test<fnv1a_32, big_endian_flavor>( -3.14f, { 0xC0, 0x48, 0xF5, 0xC3 } );

    test<fnv1a_32, big_endian_flavor>( +std::numeric_limits<float>::infinity(), { 0x7F, 0x80, 0x00, 0x00 } );
    test<fnv1a_32, big_endian_flavor>( -std::numeric_limits<float>::infinity(), { 0xFF, 0x80, 0x00, 0x00 } );

    test<fnv1a_32, little_endian_flavor>( +1.0f, { 0x00, 0x00, 0x80, 0x3F } );
    test<fnv1a_32, little_endian_flavor>( -1.0f, { 0x00, 0x00, 0x80, 0xBF } );

    test<fnv1a_32, little_endian_flavor>( +3.14f, { 0xC3, 0xF5, 0x48, 0x40 } );
    test<fnv1a_32, little_endian_flavor>( -3.14f, { 0xC3, 0xF5, 0x48, 0xC0 } );

    test<fnv1a_32, little_endian_flavor>( +std::numeric_limits<float>::infinity(), { 0x00, 0x00, 0x80, 0x7F } );
    test<fnv1a_32, little_endian_flavor>( -std::numeric_limits<float>::infinity(), { 0x00, 0x00, 0x80, 0xFF } );

    // double

    test<fnv1a_32, default_flavor>( +0.0, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } );
    test<fnv1a_32, default_flavor>( -0.0, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } );

    test<fnv1a_32, big_endian_flavor>( +1.0, { 0x3F, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } );
    test<fnv1a_32, big_endian_flavor>( -1.0, { 0xBF, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } );

    test<fnv1a_32, big_endian_flavor>( +3.14, { 0x40, 0x09, 0x1E, 0xB8, 0x51, 0xEB, 0x85, 0x1F } );
    test<fnv1a_32, big_endian_flavor>( -3.14, { 0xC0, 0x09, 0x1E, 0xB8, 0x51, 0xEB, 0x85, 0x1F } );

    test<fnv1a_32, big_endian_flavor>( +std::numeric_limits<double>::infinity(), { 0x7F, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } );
    test<fnv1a_32, big_endian_flavor>( -std::numeric_limits<double>::infinity(), { 0xFF, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } );

    test<fnv1a_32, little_endian_flavor>( +1.0, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x3F } );
    test<fnv1a_32, little_endian_flavor>( -1.0, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0xBF } );

    test<fnv1a_32, little_endian_flavor>( +3.14, { 0x1F, 0x85, 0xEB, 0x51, 0xB8, 0x1E, 0x09, 0x40 } );
    test<fnv1a_32, little_endian_flavor>( -3.14, { 0x1F, 0x85, 0xEB, 0x51, 0xB8, 0x1E, 0x09, 0xC0 } );

    test<fnv1a_32, little_endian_flavor>( +std::numeric_limits<double>::infinity(), { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x7F } );
    test<fnv1a_32, little_endian_flavor>( -std::numeric_limits<double>::infinity(), { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0xFF } );

    // long double

    /* ... */

    return boost::report_errors();
}
