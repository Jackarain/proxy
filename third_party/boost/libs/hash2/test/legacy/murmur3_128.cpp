// Copyright 2017, 2018 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#define _CRT_SECURE_NO_WARNINGS

#include <boost/hash2/legacy/murmur3.hpp>
#include <boost/hash2/detail/write.hpp>
#include <boost/core/lightweight_test.hpp>
#include <cstdint>
#include <string>
#include <cstdio>
#include <cstddef>

template<std::size_t N> std::string to_string( std::array<unsigned char, N> const & v )
{
    std::string r;

    for( std::size_t i = 0; i < N; ++i )
    {
        char buffer[ 8 ];

        std::snprintf( buffer, sizeof( buffer ), "%02x", static_cast<int>( v[ i ] ) );

        r += buffer;
    }

    return r;
}

template<class H, class S, std::size_t N> std::string hash( char const (&s)[ N ], S seed )
{
    H h( seed );

    h.update( s, N-1 );

    return to_string( h.result() );
}

int main()
{
    // Test vectors from https://pastebin.com/k2VDbWkF

    BOOST_TEST_EQ( hash<boost::hash2::murmur3_128>( "The quick brown fox jumps over the lazy dog", 0 ), std::string( "6c1b07bc7bbc4be347939ac4a93c437a" ) );
    BOOST_TEST_EQ( hash<boost::hash2::murmur3_128>( "The quick brown fox jumps over the lazy cog", 0 ), std::string( "9a2685ff70a98c653e5c8ea6eae3fe43" ) );

    BOOST_TEST_EQ( hash<boost::hash2::murmur3_128>( "The quick brown fox jumps over the lazy dog", 0x9747B28Cu ), std::string( "213163d23b7f8a73e516c07e727345f9" ) );
    BOOST_TEST_EQ( hash<boost::hash2::murmur3_128>( "The quick brown fox jumps over the lazy cog", 0x9747B28Cu ), std::string( "94618270b057cdb83cf873585b456f55" ) );

    BOOST_TEST_EQ( hash<boost::hash2::murmur3_128>( "The quick brown fox jumps over the lazy dog", 0xC58F1A7Bu ), std::string( "ff9d0cd2ee401fac26f5efde525c9338" ) );
    BOOST_TEST_EQ( hash<boost::hash2::murmur3_128>( "The quick brown fox jumps over the lazy cog", 0xC58F1A7Bu ), std::string( "8c935c5b843839f964b24f7ad58bbccd" ) );

    return boost::report_errors();
}
