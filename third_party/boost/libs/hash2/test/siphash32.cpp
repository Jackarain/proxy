// Copyright 2017, 2018 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/siphash.hpp>
#include <boost/hash2/hash_append.hpp>
#include <boost/core/lightweight_test.hpp>
#include <cstdint>
#include <vector>
#include <list>

static const std::uint32_t vectors_sip32[64] =
{
    0x5b9f35a9,
    0xb85a4727,
    0x03a662fa,
    0x04e7fe8a,
    0x89466e2a,
    0x69b6fac5,
    0x23fc6358,
    0xc563cf8b,
    0x8f84b8d0,
    0x79e706f8,
    0x3479b094,
    0x50300808,
    0x2f87f057,
    0xff63e677,
    0x7cf8ffd6,
    0x972bfe74,
    0x84acb5d9,
    0x5b6474c4,
    0x9b8d5b46,
    0x87e3ef7b,
    0x45104de3,
    0xb3623f61,
    0xfe67f370,
    0xbdb8ade6,
    0x630c4027,
    0x75787826,
    0x5f7b564f,
    0x69e6b03a,
    0x004064b0,
    0xb40f67ff,
    0x8b339e50,
    0x1a9f585d,
    0x1221e7fe,
    0x59327533,
    0x8c4f436a,
    0x29b728fe,
    0xecc65ce7,
    0x548d7e69,
    0x0f8b6863,
    0xb4620b65,
    0x4018bcb6,
    0x0545075d,
    0x2efd4224,
    0x3a86b77b,
    0x48d50577,
    0xb10852d7,
    0xc899d4b6,
    0x2e209208,
    0xe32ce169,
    0xe580b58d,
    0xc6649736,
    0x04026e01,
    0xd4f3853b,
    0xbe66dbfe,
    0x3a2a691e,
    0xc08489c6,
    0x40b9c5a5,
    0x8ce8e99b,
    0x4081bc7d,
    0xc58e077c,
    0x736ce7d4,
    0xb9cb8f42,
    0x7a9983bd,
    0x744aea59,
};

int main()
{
    unsigned char k[ 8 ];

    for( int i = 0; i < 8; ++i )
    {
        k[ i ] = static_cast<unsigned char>( i );
    }

    {
        unsigned char in[ 64 ];

        for( int i = 0; i < 64; ++i )
        {
            in[ i ] = static_cast<unsigned char>( i );

            boost::hash2::siphash_32 h( k, 8 );

            h.update( in, i );

            BOOST_TEST_EQ( h.result(), vectors_sip32[ i ] );
        }
    }

    {
        unsigned char in[ 64 ];

        for( int i = 0; i < 64; ++i )
        {
            in[ i ] = static_cast<unsigned char>( i );

            boost::hash2::siphash_32 h( k, 8 );

            hash_append_range( h, {}, in, in + i );

            BOOST_TEST_EQ( h.result(), vectors_sip32[ i ] );
        }
    }

    {
        std::vector<unsigned char> in;

        for( int i = 0; i < 64; ++i )
        {
            boost::hash2::siphash_32 h( k, 8 );

            hash_append_range( h, {}, in.begin(), in.end() );

            BOOST_TEST_EQ( h.result(), vectors_sip32[ i ] );

            in.push_back( static_cast<unsigned char>( i ) );
        }
    }

    {
        std::list<unsigned char> in;

        for( int i = 0; i < 64; ++i )
        {
            boost::hash2::siphash_32 h( k, 8 );

            hash_append_range( h, {}, in.begin(), in.end() );

            BOOST_TEST_EQ( h.result(), vectors_sip32[ i ] );

            in.push_back( static_cast<unsigned char>( i ) );
        }
    }

    return boost::report_errors();
}
