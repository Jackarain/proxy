// Copyright 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/fnv1a.hpp>
#include <boost/hash2/xxhash.hpp>
#include <boost/hash2/siphash.hpp>
#include <boost/core/lightweight_test.hpp>
#include <vector>
#include <cstddef>
#include <cstdio>

template<class H> void test_bit_flip( unsigned char ch )
{
    int const N = 32;

    std::vector<unsigned char> v( N, static_cast<unsigned char>( ch ) );

    for( int n = 1; n <= N; ++n )
    {
        typename H::result_type r1 = {};

        {
            H h;
            h.update( v.data(), n );

            r1 = h.result();
        }

        for( int j = 0; j < n * 8; ++j )
        {
            v[ j / 8 ] ^= 1 << ( j % 8 );

            H h;
            h.update( v.data(), n );

            typename H::result_type r2 = h.result();

            v[ j / 8 ] ^= 1 << ( j % 8 );

            BOOST_TEST( r1 != r2 ) || std::fprintf( stderr, "Hash collision with bit %d flipped in %d * 0x%02X\n", j, n, ch );
        }
    }
}

template<class H> void test_seq_length( unsigned char ch )
{
    int const N = 1024;

    std::vector<unsigned char> v( N, static_cast<unsigned char>( ch ) );

    typename H::result_type r[ N ] = {};

    for( int n = 0; n < N; ++n )
    {
        H h;
        h.update( v.data(), n );

        r[ n ] = h.result();

        for( int j = 0; j < n; ++j )
        {
            BOOST_TEST( r[j] != r[n] ) || std::fprintf( stderr, "Hash collision between %d * 0x%02X and %d * 0x%02X\n", j, ch, n, ch );
        }
    }
}

template<class H> void test()
{
    for( int ch = 0; ch <= 0xFF; ++ch )
    {
        test_bit_flip<H>( static_cast<unsigned char>( ch ) );
        test_seq_length<H>( static_cast<unsigned char>( ch ) );
    }
}

int main()
{
    test<boost::hash2::fnv1a_32>();
    test<boost::hash2::fnv1a_64>();
    test<boost::hash2::xxhash_32>();
    test<boost::hash2::xxhash_64>();
    test<boost::hash2::siphash_32>();
    test<boost::hash2::siphash_64>();

    return boost::report_errors();
}
