// Copyright 2017, 2018 Peter Dimov.
// Copyright 2024 Christian Mazakas.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#define _CRT_SECURE_NO_WARNINGS

#include <boost/hash2/ripemd.hpp>
#include <boost/core/lightweight_test.hpp>
#include <cstdint>
#include <string>
#include <cstddef>
#include <cstdio>

// https://homes.esat.kuleuven.be/~bosselae/ripemd160/
// ^ includes ripemd-160 and ripemd-128 test vectors

template<class H> std::string digest( std::string const & s )
{
    H h;

    h.update( s.data(), s.size() );

    return to_string( h.result() );
}

using boost::hash2::ripemd_160;
using boost::hash2::ripemd_128;

static void test_ripemd_160()
{
        std::string repeating_digits;
        for( int i = 0; i < 8; ++i )
        {
            repeating_digits += "1234567890";
        }

        // Test vectors from https://en.wikipedia.org/wiki/RIPEMD

        BOOST_TEST_EQ( digest<ripemd_160>( "" ), std::string( "9c1185a5c5e9fc54612808977ee8f548b2258d31" ) );
        BOOST_TEST_EQ( digest<ripemd_160>( "The quick brown fox jumps over the lazy dog" ), std::string( "37f332f68db77bd9d7edd4969571ad671cf9dd3b" ) );
        BOOST_TEST_EQ( digest<ripemd_160>( "The quick brown fox jumps over the lazy cog" ), std::string( "132072df690933835eb8b6ad0b77e7b6f14acad7" ) );

        // Test vectors from https://homes.esat.kuleuven.be/~bosselae/ripemd160/pdf/AB-9601/AB-9601.pdf

        BOOST_TEST_EQ( std::string( "0bdc9d2d256b3ee9daae347be6f4dc835a467ffe" ), digest<ripemd_160>( "a" ) );
        BOOST_TEST_EQ( std::string( "8eb208f7e05d987a9b044a8e98c6b087f15a0bfc" ), digest<ripemd_160>( "abc" ) );
        BOOST_TEST_EQ( std::string( "5d0689ef49d2fae572b881b123a85ffa21595f36" ), digest<ripemd_160>( "message digest" ) );
        BOOST_TEST_EQ( std::string( "f71c27109c692c1b56bbdceb5b9d2865b3708dbc" ), digest<ripemd_160>( "abcdefghijklmnopqrstuvwxyz" ) );
        BOOST_TEST_EQ( std::string( "12a053384a9c0c88e405a06c27dcf49ada62eb2b" ), digest<ripemd_160>( "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq" ) );
        BOOST_TEST_EQ( std::string( "b0e20b6e3116640286ed3a87a5713079b21f5189" ), digest<ripemd_160>( "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789" ) );
        BOOST_TEST_EQ( std::string( "9b752e45573d4b39f4dbd3323cab82bf63326bfb" ), digest<ripemd_160>( repeating_digits ) );
        BOOST_TEST_EQ( std::string( "52783243c1697bdbe16d37f97f68f08325dc1528" ), digest<ripemd_160>( std::string( 1000000, 'a' ) ) );
}

static void test_ripemd_128()
{
        std::string repeating_digits;
        for( int i = 0; i < 8; ++i )
        {
            repeating_digits += "1234567890";
        }

        // Test vectors from https://homes.esat.kuleuven.be/~bosselae/ripemd160/pdf/AB-9601/AB-9601.pdf

        BOOST_TEST_EQ( std::string( "cdf26213a150dc3ecb610f18f6b38b46" ), digest<ripemd_128>( "" ) );
        BOOST_TEST_EQ( std::string( "86be7afa339d0fc7cfc785e72f578d33" ), digest<ripemd_128>( "a" ) );
        BOOST_TEST_EQ( std::string( "c14a12199c66e4ba84636b0f69144c77" ), digest<ripemd_128>( "abc" ) );
        BOOST_TEST_EQ( std::string( "9e327b3d6e523062afc1132d7df9d1b8" ), digest<ripemd_128>( "message digest" ) );
        BOOST_TEST_EQ( std::string( "fd2aa607f71dc8f510714922b371834e" ), digest<ripemd_128>( "abcdefghijklmnopqrstuvwxyz" ) );
        BOOST_TEST_EQ( std::string( "a1aa0689d0fafa2ddc22e88b49133a06" ), digest<ripemd_128>( "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq" ) );
        BOOST_TEST_EQ( std::string( "d1e959eb179c911faea4624c60c5c702" ), digest<ripemd_128>( "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789" ) );
        BOOST_TEST_EQ( std::string( "3f45ef194732c2dbb2c4a2c769795fa3" ), digest<ripemd_128>( repeating_digits ) );
        BOOST_TEST_EQ( std::string( "4a7f5723f954eba1216c9d8f6320431f" ), digest<ripemd_128>( std::string( 1000000, 'a' ) ) );
}

int main()
{
    test_ripemd_160();
    test_ripemd_128();

    return boost::report_errors();
}
