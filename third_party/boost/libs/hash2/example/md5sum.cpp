// Copyright 2024 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#define _CRT_SECURE_NO_WARNINGS

#include <boost/hash2/md5.hpp>
#include <array>
#include <string>
#include <cerrno>
#include <cstdio>

static void md5sum( std::FILE* f, char const* fn )
{
    boost::hash2::md5_128 hash;

    int const N = 4096;
    unsigned char buffer[ N ];

    for( ;; )
    {
        std::size_t n = std::fread( buffer, 1, N, f );

        if( std::ferror( f ) )
        {
            std::fprintf( stderr, "'%s': read error: %s\n", fn, std::strerror( errno ) );
            return;
        }

        if( n == 0 ) break;

        hash.update( buffer, n );
    }

    std::string digest = to_string( hash.result() );

    std::printf( "%s *%s\n", digest.c_str(), fn );
}

int main( int argc, char const* argv[] )
{
    for( int i = 1; i < argc; ++i )
    {
        std::FILE* f = std::fopen( argv[i], "rb" );

        if( f == 0 )
        {
            std::fprintf( stderr, "'%s': open error: %s\n", argv[i], std::strerror( errno ) );
            continue;
        }

        md5sum( f, argv[i] );

        std::fclose( f );
    }
}
