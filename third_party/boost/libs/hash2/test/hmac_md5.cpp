// Copyright 2017, 2018 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#define _CRT_SECURE_NO_WARNINGS

#ifdef _MSC_VER
# pragma warning(disable: 4309) // truncation of constant value
#endif

#include <boost/hash2/md5.hpp>
#include <boost/core/lightweight_test.hpp>
#include <cstdint>
#include <string>
#include <cstddef>
#include <cstdio>

template<class H> std::string digest( std::string const k, std::string const & s )
{
    H h( k.data(), k.size() );

    h.update( s.data(), s.size() );

    return to_string( h.result() );
}

using boost::hash2::hmac_md5_128;

int main()
{
    // Test vectors from https://en.wikipedia.org/wiki/Hash-based_message_authentication_code

    BOOST_TEST_EQ( digest<hmac_md5_128>( "", "" ), std::string( "74e6f7298a9c2d168935f58c001bad88" ) );
    BOOST_TEST_EQ( digest<hmac_md5_128>( "key", "The quick brown fox jumps over the lazy dog" ), std::string( "80070713463e7749b90c2dc24911e275" ) );

    // Test vectors from https://tools.ietf.org/html/rfc2202

    BOOST_TEST_EQ( digest<hmac_md5_128>( std::string( 16, 0x0B ), "Hi There" ), std::string( "9294727a3638bb1c13f48ef8158bfc9d" ) );
    BOOST_TEST_EQ( digest<hmac_md5_128>( "Jefe", "what do ya want for nothing?" ), std::string( "750c783e6ab0b503eaa86e310a5db738" ) );
    BOOST_TEST_EQ( digest<hmac_md5_128>( std::string( 16, 0xAA ), std::string( 50, 0xDD ) ), std::string( "56be34521d144c88dbb8c733f0e8b3f6" ) );
    BOOST_TEST_EQ( digest<hmac_md5_128>( "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19", std::string( 50, 0xCD ) ), std::string( "697eaf0aca3a3aea3a75164746ffaa79" ) );
    BOOST_TEST_EQ( digest<hmac_md5_128>( std::string( 16, 0x0C ), "Test With Truncation" ), std::string( "56461ef2342edc00f9bab995690efd4c" ) );
    BOOST_TEST_EQ( digest<hmac_md5_128>( std::string( 80, 0xAA ), "Test Using Larger Than Block-Size Key - Hash Key First" ), std::string( "6b1ab7fe4bd7bf8f0b62e6ce61b9d0cd" ) );
    BOOST_TEST_EQ( digest<hmac_md5_128>( std::string( 80, 0xAA ), "Test Using Larger Than Block-Size Key and Larger Than One Block-Size Data" ), std::string( "6f630fad67cda0ee1fb1f562db3aa53e" ) );

    return boost::report_errors();
}
