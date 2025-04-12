// Copyright 2024 Christian Mazakas
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/sha3.hpp>
#include <boost/hash2/digest.hpp>
#include <boost/hash2/detail/config.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <boost/config/workaround.hpp>

#if defined(BOOST_MSVC) && BOOST_MSVC < 1920
# pragma warning(disable: 4307) // integral constant overflow
#endif

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

// gcc-5 has issues with these constexpr tests:
// libs/hash2/test/sha3_cx.cpp:100:132: internal compiler error: in fold_binary_loc, at fold-const.c:9925
//          TEST_EQ( test<sha3_256>( 0, str1 ), digest_from_hex( "3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532" ) );
#if defined(BOOST_NO_CXX14_CONSTEXPR) || BOOST_WORKAROUND(BOOST_GCC, < 60000)
# define TEST_EQ(x1, x2) BOOST_TEST_EQ(x1, x2)
#else
# define TEST_EQ(x1, x2) BOOST_TEST_EQ(x1, x2); STATIC_ASSERT( x1 == x2 )
#endif

template<class H, std::size_t N> BOOST_CXX14_CONSTEXPR typename H::result_type test( std::uint64_t seed, char const (&str)[ N ] )
{
    H h( seed );

    std::size_t const M = N - 1; // strip off null-terminator

    // only update( unsigned char const*, std::size_t ) is constexpr so we memcpy here to emulate bit_cast
    unsigned char buf[ N ] = {}; // avoids zero-sized arrays
    for( unsigned i = 0; i < M; ++i ){ buf[i] = str[i]; }

    h.update( buf, M / 3 );
    h.update( buf + M / 3, M - M / 3 );

    return h.result();
}

template<class H, std::size_t N> BOOST_CXX14_CONSTEXPR typename H::result_type test( std::uint64_t seed, unsigned char const (&buf)[ N ] )
{
    H h( seed );

    h.update( buf, N / 3 );
    h.update( buf + N / 3, N - N / 3 );

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

template<class H, std::size_t N> BOOST_CXX14_CONSTEXPR typename H::result_type test_hex( std::uint64_t seed, char const (&str)[ N ] )
{
    H h( seed );

    std::size_t const M = ( N - 1 ) / 2;

    unsigned char buf[M] = {};
    for( unsigned i = 0; i < M; ++i ) {
        auto c1 = to_byte( str[ 2 * i ] );
        auto c2 = to_byte( str[ 2 * i + 1 ] );
        buf[ i ] = ( c1 << 4 ) | c2;
    }

    h.update( buf, M / 3 );
    h.update( buf + M / 3, M - M / 3 );

    return h.result();
}

int main()
{
    using namespace boost::hash2;

    {
        constexpr char const str1[] = "abc";
        constexpr char const str2[] = "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu";

        TEST_EQ( test<sha3_384>( 0, str1 ), digest_from_hex( "ec01498288516fc926459f58e2c6ad8df9b473cb0fc08c2596da7cf0e49be4b298d88cea927ac7f539f1edf228376d25" ) );
        TEST_EQ( test<sha3_384>( 7, str1 ), digest_from_hex( "c16d70af9d4e75ad91fd0dfec80e52bd2dbad910d5afc36b424b5280755409e61a117a937a5872ccfd228b42a0153dde" ) );

        TEST_EQ( test<sha3_384>( 0, str2 ), digest_from_hex( "79407d3b5916b59c3e30b09822974791c313fb9ecc849e406f23592d04f625dc8c709b98b43b3852b337216179aa7fc7" ) );
#if !BOOST_WORKAROUND(BOOST_MSVC, < 1920)
        TEST_EQ( test<sha3_384>( 7, str2 ), digest_from_hex( "01d50e968b05de4a3f35eb9c2ee626fb16c81d1c55d0a1cfdc1e5170ed33e8eb8a907f32614ccb3345df600f7f02ed1a" ) );
#endif
    }

    {
        constexpr auto const N = sha3_384::block_size;

        constexpr char const buf1[] = "";
        constexpr unsigned char buf2[ N - 1 ] = {};
        constexpr unsigned char buf3[ N ] = {};
        constexpr unsigned char buf4[ N + 1 ] = {};

        TEST_EQ( test<sha3_384>( 0, buf1 ), digest_from_hex( "0c63a75b845e4f7d01107d852e4c2485c51a50aaaa94fc61995e71bbee983a2ac3713831264adb47fb6bd1e058d5f004" ) );
        TEST_EQ( test<sha3_384>( 0, buf2 ), digest_from_hex( "11c556552dda63418669716bad02e4125f4973f3ceea99ee50b6ff117e9f7a3fed0360abb5eff4ac8e954205c01981d2" ) );
        TEST_EQ( test<sha3_384>( 0, buf3 ), digest_from_hex( "aaed6beb61b1f9a9b469d38a27a35edde7f676f4603e67f5424c7588043b869ebbfcfc3ecee2ae6f5ecfaf7f706c49e3" ) );
        TEST_EQ( test<sha3_384>( 0, buf4 ), digest_from_hex( "7db7a10350831a0b3c8c94a138a301858dd8c6d589cd1b47f6720f9243162f952161ae945ec8cf7a838d02cfbcc762ee" ) );

        TEST_EQ( test<sha3_384>( 7, buf1 ), digest_from_hex( "487bccce1cc1c6c87224fd6b978ab74c29454ac6375870679ca488832190459a0ae6426d0032fa87b28b5190358a8fc1" ) );
        TEST_EQ( test<sha3_384>( 7, buf2 ), digest_from_hex( "d6a9b3306e845e761ebd48d1cf06f7a0d86ac087ff61b236d62b7544ffd7760e1ef46f2b4845e72a0faaa91d12c096b6" ) );
#if !BOOST_WORKAROUND(BOOST_MSVC, < 1920)
        TEST_EQ( test<sha3_384>( 7, buf3 ), digest_from_hex( "d2bebc43151543402151fd9a6d9c4dba50d582cd035a5aecd39fe957fe9a46a77c8e4278f4d8f6548d719439b24416fa" ) );
        TEST_EQ( test<sha3_384>( 7, buf4 ), digest_from_hex( "81fd37d2cdf0bed5d6ee8d5a63749a0ea7463073c9ccbe0d0bff904d5c518ac685650a699ac23ce3ddf6e81a65bea86b" ) );
#endif
    }

    return boost::report_errors();
}
