// Copyright 2024 Christian Mazakas
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/ripemd.hpp>
#include <boost/hash2/digest.hpp>
#include <boost/hash2/hash_append.hpp>
#include <boost/hash2/detail/config.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>

#if defined(BOOST_MSVC) && BOOST_MSVC < 1920
# pragma warning(disable: 4307) // integral constant overflow
#endif

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

#if defined(BOOST_NO_CXX14_CONSTEXPR)
# define TEST_EQ(x1, x2) BOOST_TEST_EQ(x1, x2)
#else
# define TEST_EQ(x1, x2) BOOST_TEST_EQ(x1, x2); STATIC_ASSERT(x1 == x2)
#endif

template<class H, std::size_t N> BOOST_CXX14_CONSTEXPR typename H::result_type test( std::uint64_t seed, char const (&str)[ N ] )
{
    H h( seed );

    std::size_t const M = N - 1; // strip off null-terminator

    #if BOOST_WORKAROUND(BOOST_GCC, >= 50000 && BOOST_GCC < 60000)
        unsigned char buf[M] = {};
        for( unsigned i = 0; i < M; ++i ) buf[ i ] = str[ i ];

        h.update( buf, M / 3 );
        h.update( buf + M / 3, M - M / 3 );
    #else
        boost::hash2::hash_append_range( h, boost::hash2::default_flavor{}, str, str + M / 3 );
        boost::hash2::hash_append_range( h, boost::hash2::default_flavor{}, str + M / 3, str + M );
    #endif

    return h.result();
}

BOOST_CXX14_CONSTEXPR unsigned char to_byte( char c )
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0xff;
}

template<std::size_t N, std::size_t M = ( N - 1 ) / 2>
BOOST_CXX14_CONSTEXPR boost::hash2::digest<M> digest_from_hex( char const (&str)[ N ] )
{
    boost::hash2::digest<M> dgst = {};
    auto* p = dgst.data();
    for( unsigned i = 0; i < M; ++i ) {
        auto c1 = to_byte( str[ 2 * i ] );
        auto c2 = to_byte( str[ 2 * i + 1 ] );
        p[ i ] = ( c1 << 4 ) | c2;
    }
    return dgst;
}

int main()
{
    using namespace boost::hash2;

    TEST_EQ( digest_from_hex( "0bdc9d2d256b3ee9daae347be6f4dc835a467ffe" ), test<ripemd_160>( 0, "a" ) );
    TEST_EQ( digest_from_hex( "8eb208f7e05d987a9b044a8e98c6b087f15a0bfc" ), test<ripemd_160>( 0, "abc" ) );
    TEST_EQ( digest_from_hex( "5d0689ef49d2fae572b881b123a85ffa21595f36" ), test<ripemd_160>( 0, "message digest" ) );
    TEST_EQ( digest_from_hex( "f71c27109c692c1b56bbdceb5b9d2865b3708dbc" ), test<ripemd_160>( 0, "abcdefghijklmnopqrstuvwxyz" ) );
    TEST_EQ( digest_from_hex( "12a053384a9c0c88e405a06c27dcf49ada62eb2b" ), test<ripemd_160>( 0, "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq" ) );
    TEST_EQ( digest_from_hex( "b0e20b6e3116640286ed3a87a5713079b21f5189" ), test<ripemd_160>( 0, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789" ) );

    TEST_EQ( digest_from_hex( "7580d81d5ed1467317ce3ab9199088764a518b9d" ), test<ripemd_160>( 7, "a" ) );
    TEST_EQ( digest_from_hex( "f5a724fe56b405db4edcb1ea46707ad78753a2e4" ), test<ripemd_160>( 7, "abc" ) );
    TEST_EQ( digest_from_hex( "da6a32ce82c07de14ac5c2be1be3775201f07941" ), test<ripemd_160>( 7, "message digest" ) );
    TEST_EQ( digest_from_hex( "221e3b3af0671febf816397ded6f05c8a5c540d3" ), test<ripemd_160>( 7, "abcdefghijklmnopqrstuvwxyz" ) );
    TEST_EQ( digest_from_hex( "4578ce76d4ddec5a75288306da83f65eb963973c" ), test<ripemd_160>( 7, "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq" ) );
    TEST_EQ( digest_from_hex( "868f80dffc5c55612afdd5f8c7c4948410134dc5" ), test<ripemd_160>( 7, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789" ) );

    TEST_EQ( digest_from_hex( "86be7afa339d0fc7cfc785e72f578d33" ), test<ripemd_128>( 0, "a" ) );
    TEST_EQ( digest_from_hex( "c14a12199c66e4ba84636b0f69144c77" ), test<ripemd_128>( 0, "abc" ) );
    TEST_EQ( digest_from_hex( "9e327b3d6e523062afc1132d7df9d1b8" ), test<ripemd_128>( 0, "message digest" ) );
    TEST_EQ( digest_from_hex( "fd2aa607f71dc8f510714922b371834e" ), test<ripemd_128>( 0, "abcdefghijklmnopqrstuvwxyz" ) );
    TEST_EQ( digest_from_hex( "a1aa0689d0fafa2ddc22e88b49133a06" ), test<ripemd_128>( 0, "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq" ) );
    TEST_EQ( digest_from_hex( "d1e959eb179c911faea4624c60c5c702" ), test<ripemd_128>( 0, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789" ) );

    TEST_EQ( digest_from_hex( "f27337949d37e6be3d72a8426fa294e4" ), test<ripemd_128>( 7, "a" ) );
    TEST_EQ( digest_from_hex( "8d786948560425241971dac7a9cf8411" ), test<ripemd_128>( 7, "abc" ) );
    TEST_EQ( digest_from_hex( "dc0be47779611aaf5d3f01c542a47c8d" ), test<ripemd_128>( 7, "message digest" ) );
    TEST_EQ( digest_from_hex( "55d084891941b4957e5708a19a20205d" ), test<ripemd_128>( 7, "abcdefghijklmnopqrstuvwxyz" ) );
    TEST_EQ( digest_from_hex( "6f1c8c55ef315a5672f023d477978e04" ), test<ripemd_128>( 7, "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq" ) );
    TEST_EQ( digest_from_hex( "02e8ba3888dc72487d45ef5e2332903f" ), test<ripemd_128>( 7, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789" ) );

    return boost::report_errors();
}
