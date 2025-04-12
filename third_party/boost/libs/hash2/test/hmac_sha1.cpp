// Copyright 2017, 2018 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#define _CRT_SECURE_NO_WARNINGS

#ifdef _MSC_VER
# pragma warning(disable: 4309) // truncation of constant value
#endif

#include <boost/hash2/sha1.hpp>
#include <boost/core/lightweight_test.hpp>
#include <string>

template<class H> std::string digest( std::string const & k, std::string const & s )
{
    H h( k.data(), k.size() );

    h.update( s.data(), s.size() );

    return to_string( h.result() );
}

using boost::hash2::hmac_sha1_160;

int main()
{
    // Test vectors from https://en.wikipedia.org/wiki/Hash-based_message_authentication_code

    BOOST_TEST_EQ( digest<hmac_sha1_160>( "", "" ), std::string( "fbdb1d1b18aa6c08324b7d64b71fb76370690e1d" ) );
    BOOST_TEST_EQ( digest<hmac_sha1_160>( "key", "The quick brown fox jumps over the lazy dog" ), std::string( "de7c9b85b8b78aa6bc8a7a36f70a90701c9db4d9" ) );

    // Test vectors from https://tools.ietf.org/html/rfc2202

    BOOST_TEST_EQ( digest<hmac_sha1_160>( std::string( 20, 0x0B ), "Hi There" ), std::string( "b617318655057264e28bc0b6fb378c8ef146be00" ) );
    BOOST_TEST_EQ( digest<hmac_sha1_160>( "Jefe", "what do ya want for nothing?" ), std::string( "effcdf6ae5eb2fa2d27416d5f184df9c259a7c79" ) );
    BOOST_TEST_EQ( digest<hmac_sha1_160>( std::string( 20, 0xAA ), std::string( 50, 0xDD ) ), std::string( "125d7342b9ac11cd91a39af48aa17b4f63f175d3" ) );
    BOOST_TEST_EQ( digest<hmac_sha1_160>( "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19", std::string( 50, 0xCD ) ), std::string( "4c9007f4026250c6bc8414f9bf50c86c2d7235da" ) );
    BOOST_TEST_EQ( digest<hmac_sha1_160>( std::string( 20, 0x0C ), "Test With Truncation" ), std::string( "4c1a03424b55e07fe7f27be1d58bb9324a9a5a04" ) );
    BOOST_TEST_EQ( digest<hmac_sha1_160>( std::string( 80, 0xAA ), "Test Using Larger Than Block-Size Key - Hash Key First" ), std::string( "aa4ae5e15272d00e95705637ce8a3b55ed402112" ) );
    BOOST_TEST_EQ( digest<hmac_sha1_160>( std::string( 80, 0xAA ), "Test Using Larger Than Block-Size Key and Larger Than One Block-Size Data" ), std::string( "e8e99d0f45237d786d6bbaa7965c7808bbff1a91" ) );

    return boost::report_errors();
}
