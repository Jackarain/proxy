// Copyright 2017, 2018 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#define _CRT_SECURE_NO_WARNINGS

#include <boost/hash2/md5.hpp>
#include <boost/core/lightweight_test.hpp>
#include <cstdint>
#include <string>
#include <cstddef>
#include <cstdio>

template<class H> std::string digest( char const * s )
{
    H h;

    h.update( s, std::strlen( s ) );

    return to_string( h.result() );
}

using boost::hash2::md5_128;

int main()
{
    // Test vectors from https://tools.ietf.org/html/rfc1321

    BOOST_TEST_EQ( digest<md5_128>( "" ), std::string( "d41d8cd98f00b204e9800998ecf8427e" ) );
    BOOST_TEST_EQ( digest<md5_128>( "a" ), std::string( "0cc175b9c0f1b6a831c399e269772661" ) );
    BOOST_TEST_EQ( digest<md5_128>( "abc" ), std::string( "900150983cd24fb0d6963f7d28e17f72" ) );
    BOOST_TEST_EQ( digest<md5_128>( "message digest" ), std::string( "f96b697d7cb7938d525a2f31aaf161d0" ) );
    BOOST_TEST_EQ( digest<md5_128>( "abcdefghijklmnopqrstuvwxyz" ), std::string( "c3fcd3d76192e4007dfb496cca67e13b" ) );
    BOOST_TEST_EQ( digest<md5_128>( "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789" ), std::string( "d174ab98d277d9f5a5611c2c9f419d9f" ) );
    BOOST_TEST_EQ( digest<md5_128>( "12345678901234567890123456789012345678901234567890123456789012345678901234567890" ), std::string( "57edf4a22be3c955ac49da2e2107b67a" ) );

    return boost::report_errors();
}
