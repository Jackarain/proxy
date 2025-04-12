// Copyright 2024 Christian Mazakas.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#define _CRT_SECURE_NO_WARNINGS

#ifdef _MSC_VER
# pragma warning(disable: 4309) // truncation of constant value
#endif

#include <boost/hash2/ripemd.hpp>
#include <boost/core/lightweight_test.hpp>
#include <string>
#include <cstddef>
#include <cstdio>

template<class H> std::string digest( std::string const k, std::string const & s )
{
    H h( k.data(), k.size() );

    h.update( s.data(), s.size() );

    return to_string( h.result() );
}

std::string from_hex( char const* str )
{
    auto f = []( char c ) { return ( c >= 'a' ? c - 'a' + 10 : c - '0' ); };

    std::string s;
    while( *str != '\0' )
    {
        s.push_back( static_cast<char>( ( f( str[ 0 ] ) << 4 ) + f( str[ 1 ] ) ) );
        str += 2;
    }
    return s;
}

using boost::hash2::hmac_ripemd_160;
using boost::hash2::hmac_ripemd_128;

int main()
{
    // Test vectors from https://datatracker.ietf.org/doc/rfc2286/

    BOOST_TEST_EQ( std::string( "24cb4bd67d20fc1a5d2ed7732dcc39377f0a5668" ), digest<hmac_ripemd_160>( from_hex( "0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b" ), "Hi There" ) );
    BOOST_TEST_EQ( std::string( "dda6c0213a485a9e24f4742064a7f033b43c4069" ), digest<hmac_ripemd_160>( "Jefe", "what do ya want for nothing?" ) );
    BOOST_TEST_EQ( std::string( "b0b105360de759960ab4f35298e116e295d8e7c1" ), digest<hmac_ripemd_160>( from_hex( "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" ), from_hex( "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd" ) ) );
    BOOST_TEST_EQ( std::string( "d5ca862f4d21d5e610e18b4cf1beb97a4365ecf4" ), digest<hmac_ripemd_160>( from_hex( "0102030405060708090a0b0c0d0e0f10111213141516171819" ), from_hex( "cdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcd" ) ) );
    BOOST_TEST_EQ( std::string( "7619693978f91d90539ae786500ff3d8e0518e39" ), digest<hmac_ripemd_160>( from_hex( "0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c" ), "Test With Truncation" ) );
    BOOST_TEST_EQ( std::string( "6466ca07ac5eac29e1bd523e5ada7605b791fd8b" ), digest<hmac_ripemd_160>( from_hex( "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" ), "Test Using Larger Than Block-Size Key - Hash Key First" ) );
    BOOST_TEST_EQ( std::string( "69ea60798d71616cce5fd0871e23754cd75d5a0a" ), digest<hmac_ripemd_160>( from_hex( "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" ), "Test Using Larger Than Block-Size Key and Larger Than One Block-Size Data" ) );

    // Test vectors from https://homes.esat.kuleuven.be/%7Ebosselae/ripemd160.html

    BOOST_TEST_EQ( std::string( "cf387677bfda8483e63b57e06c3b5ecd8b7fc055" ), digest<hmac_ripemd_160>( from_hex( "00112233445566778899aabbccddeeff01234567" ), "" ) );
    BOOST_TEST_EQ( std::string( "fe69a66c7423eea9c8fa2eff8d9dafb4f17a62f5" ), digest<hmac_ripemd_160>( from_hex( "0123456789abcdeffedcba987654321000112233" ), "" ) );
    BOOST_TEST_EQ( std::string( "0d351d71b78e36dbb7391c810a0d2b6240ddbafc" ), digest<hmac_ripemd_160>( from_hex( "00112233445566778899aabbccddeeff01234567" ), "a" ) );
    BOOST_TEST_EQ( std::string( "85743e899bc82dbfa36faaa7a25b7cfd372432cd" ), digest<hmac_ripemd_160>( from_hex( "0123456789abcdeffedcba987654321000112233" ), "a" ) );
    BOOST_TEST_EQ( std::string( "f7ef288cb1bbcc6160d76507e0a3bbf712fb67d6" ), digest<hmac_ripemd_160>( from_hex( "00112233445566778899aabbccddeeff01234567" ), "abc" ) );
    BOOST_TEST_EQ( std::string( "6e4afd501fa6b4a1823ca3b10bd9aa0ba97ba182" ), digest<hmac_ripemd_160>( from_hex( "0123456789abcdeffedcba987654321000112233" ), "abc" ) );
    BOOST_TEST_EQ( std::string( "f83662cc8d339c227e600fcd636c57d2571b1c34" ), digest<hmac_ripemd_160>( from_hex( "00112233445566778899aabbccddeeff01234567" ), "message digest"  ) );
    BOOST_TEST_EQ( std::string( "2e066e624badb76a184c8f90fba053330e650e92" ), digest<hmac_ripemd_160>( from_hex( "0123456789abcdeffedcba987654321000112233" ), "message digest"  ) );
    BOOST_TEST_EQ( std::string( "843d1c4eb880ac8ac0c9c95696507957d0155ddb" ), digest<hmac_ripemd_160>( from_hex( "00112233445566778899aabbccddeeff01234567" ), "abcdefghijklmnopqrstuvwxyz" ) );
    BOOST_TEST_EQ( std::string( "07e942aa4e3cd7c04dedc1d46e2e8cc4c741b3d9" ), digest<hmac_ripemd_160>( from_hex( "0123456789abcdeffedcba987654321000112233" ), "abcdefghijklmnopqrstuvwxyz" ) );
    BOOST_TEST_EQ( std::string( "60f5ef198a2dd5745545c1f0c47aa3fb5776f881" ), digest<hmac_ripemd_160>( from_hex( "00112233445566778899aabbccddeeff01234567" ), "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq" ) );
    BOOST_TEST_EQ( std::string( "b6582318ddcfb67a53a67d676b8ad869aded629a" ), digest<hmac_ripemd_160>( from_hex( "0123456789abcdeffedcba987654321000112233" ), "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq" ) );
    BOOST_TEST_EQ( std::string( "e49c136a9e5627e0681b808a3b97e6a6e661ae79" ), digest<hmac_ripemd_160>( from_hex( "00112233445566778899aabbccddeeff01234567" ), "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789" ) );
    BOOST_TEST_EQ( std::string( "f1be3ee877703140d34f97ea1ab3a07c141333e2" ), digest<hmac_ripemd_160>( from_hex( "0123456789abcdeffedcba987654321000112233" ), "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789" ) );
    BOOST_TEST_EQ( std::string( "31be3cc98cee37b79b0619e3e1c2be4f1aa56e6c" ), digest<hmac_ripemd_160>( from_hex( "00112233445566778899aabbccddeeff01234567" ), "12345678901234567890123456789012345678901234567890123456789012345678901234567890" ) );
    BOOST_TEST_EQ( std::string( "85f164703e61a63131be7e45958e0794123904f9" ), digest<hmac_ripemd_160>( from_hex( "0123456789abcdeffedcba987654321000112233" ), "12345678901234567890123456789012345678901234567890123456789012345678901234567890" ) );

    BOOST_TEST_EQ( std::string( "ad9db2c1e22af9ab5ca9dbe5a86f67dc" ), digest<hmac_ripemd_128>( from_hex( "00112233445566778899aabbccddeeff" ), "" ) );
    BOOST_TEST_EQ( std::string( "8931eeee56a6b257fd1ab5418183d826" ), digest<hmac_ripemd_128>( from_hex( "0123456789abcdeffedcba9876543210" ), "" ) );
    BOOST_TEST_EQ( std::string( "3bf448c762de00bcfa0310b11c0bde4c" ), digest<hmac_ripemd_128>( from_hex( "00112233445566778899aabbccddeeff" ), "a" ) );
    BOOST_TEST_EQ( std::string( "dbbcf169ea7419d5ba7bd8eb3673ff2d" ), digest<hmac_ripemd_128>( from_hex( "0123456789abcdeffedcba9876543210" ), "a" ) );
    BOOST_TEST_EQ( std::string( "f34ec0945f02b70b8603f89e1ce4c78c" ), digest<hmac_ripemd_128>( from_hex( "00112233445566778899aabbccddeeff" ), "abc" ) );
    BOOST_TEST_EQ( std::string( "2c4cd07d3162d6a0e338004d6b6fbc9a" ), digest<hmac_ripemd_128>( from_hex( "0123456789abcdeffedcba9876543210" ), "abc" ) );
    BOOST_TEST_EQ( std::string( "e8503a8aec2289d82aa0d8d445a06bdd" ), digest<hmac_ripemd_128>( from_hex( "00112233445566778899aabbccddeeff" ), "message digest"  ) );
    BOOST_TEST_EQ( std::string( "75bfb25888f4bb77c77ae83ad0817447" ), digest<hmac_ripemd_128>( from_hex( "0123456789abcdeffedcba9876543210" ), "message digest"  ) );
    BOOST_TEST_EQ( std::string( "ee880b735ce3126065de1699cc136199" ), digest<hmac_ripemd_128>( from_hex( "00112233445566778899aabbccddeeff" ), "abcdefghijklmnopqrstuvwxyz" ) );
    BOOST_TEST_EQ( std::string( "b1b5dc0fcb7258758855dd1840fcdce4" ), digest<hmac_ripemd_128>( from_hex( "0123456789abcdeffedcba9876543210" ), "abcdefghijklmnopqrstuvwxyz" ) );
    BOOST_TEST_EQ( std::string( "794daf2e3bdeea2538638a5ced154434" ), digest<hmac_ripemd_128>( from_hex( "00112233445566778899aabbccddeeff" ), "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq" ) );
    BOOST_TEST_EQ( std::string( "670d0f7a697b18f1a8ab7d2a2a00dbc1" ), digest<hmac_ripemd_128>( from_hex( "0123456789abcdeffedcba9876543210" ), "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq" ) );
    BOOST_TEST_EQ( std::string( "3a06eef165b23625247800be23e232b6" ), digest<hmac_ripemd_128>( from_hex( "00112233445566778899aabbccddeeff" ), "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789" ) );
    BOOST_TEST_EQ( std::string( "54e315fdb34a61c0475392e5c7852998" ), digest<hmac_ripemd_128>( from_hex( "0123456789abcdeffedcba9876543210" ), "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789" ) );
    BOOST_TEST_EQ( std::string( "9a4f0159c0952da43a8d466d46b0af58" ), digest<hmac_ripemd_128>( from_hex( "00112233445566778899aabbccddeeff" ), "12345678901234567890123456789012345678901234567890123456789012345678901234567890" ) );
    BOOST_TEST_EQ( std::string( "ad04354d8aa2a623e72e3594ee3535c0" ), digest<hmac_ripemd_128>( from_hex( "0123456789abcdeffedcba9876543210" ), "12345678901234567890123456789012345678901234567890123456789012345678901234567890" ) );

    return boost::report_errors();
}
