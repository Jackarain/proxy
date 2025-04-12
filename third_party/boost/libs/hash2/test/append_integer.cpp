// Copyright 2024 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/hash_append.hpp>
#include <boost/hash2/fnv1a.hpp>
#include <boost/core/lightweight_test.hpp>
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

    // 8 bit

    test<fnv1a_32, default_flavor>( std::int8_t( 0 ), { 0x00 } );
    test<fnv1a_32, default_flavor>( std::uint8_t( 0 ), { 0x00 } );

    test<fnv1a_32, little_endian_flavor>( std::int8_t( -2 ), { 0xFE } );
    test<fnv1a_32, little_endian_flavor>( std::uint8_t( +1 ), { 0x01 } );

    test<fnv1a_32, big_endian_flavor>( std::int8_t( -2 ), { 0xFE } );
    test<fnv1a_32, big_endian_flavor>( std::uint8_t( +1 ), { 0x01 } );

    // 16 bit

    test<fnv1a_32, default_flavor>( std::int16_t( 0 ), { 0x00, 0x00 } );
    test<fnv1a_32, default_flavor>( std::uint16_t( 0 ), { 0x00, 0x00 } );

    test<fnv1a_32, little_endian_flavor>( std::int16_t( -2 ), { 0xFE, 0xFF } );
    test<fnv1a_32, little_endian_flavor>( std::uint16_t( +1 ), { 0x01, 0x00 } );

    test<fnv1a_32, big_endian_flavor>( std::int16_t( -2 ), { 0xFF, 0xFE } );
    test<fnv1a_32, big_endian_flavor>( std::uint16_t( +1 ), { 0x00, 0x01 } );

    // 32 bit

    test<fnv1a_32, default_flavor>( std::int32_t( 0 ), { 0x00, 0x00, 0x00, 0x00 } );
    test<fnv1a_32, default_flavor>( std::uint32_t( 0 ), { 0x00, 0x00, 0x00, 0x00 } );

    test<fnv1a_32, little_endian_flavor>( std::int32_t( -2 ), { 0xFE, 0xFF, 0xFF, 0xFF } );
    test<fnv1a_32, little_endian_flavor>( std::uint32_t( +1 ), { 0x01, 0x00, 0x00, 0x00 } );

    test<fnv1a_32, big_endian_flavor>( std::int32_t( -2 ), { 0xFF, 0xFF, 0xFF, 0xFE } );
    test<fnv1a_32, big_endian_flavor>( std::uint32_t( +1 ), { 0x00, 0x00, 0x00, 0x01 } );

    // 64 bit

    test<fnv1a_32, default_flavor>( std::int64_t( 0 ), { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } );
    test<fnv1a_32, default_flavor>( std::uint64_t( 0 ), { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } );

    test<fnv1a_32, little_endian_flavor>( std::int64_t( -2 ), { 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF } );
    test<fnv1a_32, little_endian_flavor>( std::uint64_t( +1 ), { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } );

    test<fnv1a_32, big_endian_flavor>( std::int64_t( -2 ), { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE } );
    test<fnv1a_32, big_endian_flavor>( std::uint64_t( +1 ), { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 } );

    return boost::report_errors();
}
