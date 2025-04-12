// Copyright 2024 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#define _CRT_SECURE_NO_WARNINGS

#include <boost/hash2/md5.hpp>
#include <boost/hash2/sha1.hpp>
#include <boost/hash2/sha2.hpp>
#include <boost/hash2/ripemd.hpp>
#include <boost/mp11.hpp>
#include <array>
#include <string>
#include <cerrno>
#include <cstdio>

template<class Hash> void hash2sum( std::FILE* f, char const* fn )
{
    Hash hash;

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

template<class Hash> void hash2sum( char const* fn )
{
    std::FILE* f = std::fopen( fn, "rb" );

    if( f == 0 )
    {
        std::fprintf( stderr, "'%s': open error: %s\n", fn, std::strerror( errno ) );
    }
    else
    {
        hash2sum<Hash>( f, fn );
        std::fclose( f );
    }
}

using namespace boost::mp11;
using namespace boost::hash2;

using hashes = mp_list<

    md5_128,
    sha1_160,
    sha2_256,
    sha2_224,
    sha2_512,
    sha2_384,
    sha2_512_256,
    sha2_512_224,
    ripemd_160,
    ripemd_128

>;

constexpr char const* names[] = {

    "md5_128",
    "sha1_160",
    "sha2_256",
    "sha2_224",
    "sha2_512",
    "sha2_384",
    "sha2_512_256",
    "sha2_512_224",
    "ripemd_160",
    "ripemd_128"

};

int main( int argc, char const* argv[] )
{
    if( argc < 2 )
    {
        std::fputs( "usage: hash2sum <hash> <files...>\n", stderr );
        return 2;
    }

    std::string hash( argv[1] );
    bool found = false;

    mp_for_each< mp_iota<mp_size<hashes>> >([&](auto I){

        if( hash == names[I] )
        {
            using Hash = mp_at_c<hashes, I>;

            for( int i = 2; i < argc; ++i )
            {
                hash2sum<Hash>( argv[i] );
            }

            found = true;
        }

    });

    if( !found )
    {
        std::fprintf( stderr, "hash2sum: unknown hash algorithm name '%s'; use one of the following:\n\n", hash.c_str() );

        for( char const* name: names )
        {
            std::fprintf( stderr, "   %s\n", name );
        }

        return 1;
    }
}
