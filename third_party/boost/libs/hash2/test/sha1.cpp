// Copyright 2017, 2018 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#define _CRT_SECURE_NO_WARNINGS

#include <boost/hash2/sha1.hpp>
#include <boost/core/lightweight_test.hpp>
#include <string>

template<class H> std::string digest( std::string const & s )
{
    H h;
    h.update( s.data(), s.size() );

    return to_string( h.result() );
}

using boost::hash2::sha1_160;

int main()
{
    // Test vectors from https://en.wikipedia.org/wiki/SHA-1

    BOOST_TEST_EQ( digest<sha1_160>( "" ), std::string( "da39a3ee5e6b4b0d3255bfef95601890afd80709" ) );
    BOOST_TEST_EQ( digest<sha1_160>( "The quick brown fox jumps over the lazy dog" ), std::string( "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12" ) );
    BOOST_TEST_EQ( digest<sha1_160>( "The quick brown fox jumps over the lazy cog" ), std::string( "de9f2c7fd25e1b3afad3e85a0bd17d9b100db4b3" ) );

    // Test vectors from https://tools.ietf.org/html/rfc3174

    BOOST_TEST_EQ( digest<sha1_160>( "abc" ), std::string( "a9993e364706816aba3e25717850c26c9cd0d89d" ) );
    BOOST_TEST_EQ( digest<sha1_160>( "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq" ), std::string( "84983e441c3bd26ebaae4aa1f95129e5e54670f1" ) );
    BOOST_TEST_EQ( digest<sha1_160>( std::string( 1000000, 'a' ) ), std::string( "34aa973cd4c4daa4f61eeb2bdbad27316534016f" ) );

    return boost::report_errors();
}
