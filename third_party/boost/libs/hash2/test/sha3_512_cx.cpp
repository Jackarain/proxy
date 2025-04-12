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

    unsigned char buf[ N ] = {}; // avoids zero-sized arrays
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

        TEST_EQ( test<sha3_512>( 0, str1 ), digest_from_hex( "b751850b1a57168a5693cd924b6b096e08f621827444f70d884f5d0240d2712e10e116e9192af3c91a7ec57647e3934057340b4cf408d5a56592f8274eec53f0" ) );
        TEST_EQ( test<sha3_512>( 7, str1 ), digest_from_hex( "7fcdaa301f16df33e622e204cbfda936928e7b23ecd26a4efdb30c11782b7dfb079d45800015a8ca306a95ad5bc97c3aa7ed99920e1730826d8260fad8a59d33" ) );

        TEST_EQ( test<sha3_512>( 0, str2 ), digest_from_hex( "afebb2ef542e6579c50cad06d2e578f9f8dd6881d7dc824d26360feebf18a4fa73e3261122948efcfd492e74e82e2189ed0fb440d187f382270cb455f21dd185" ) );
#if !BOOST_WORKAROUND(BOOST_MSVC, < 1920)
        TEST_EQ( test<sha3_512>( 7, str2 ), digest_from_hex( "fc4a2170b4693ae60039188d07e8d49d9b4457fc42e86f3773baa123d48b5fb655bcee8d7371fa136f4dcecdfac647e7275c42b8cc96a616ce67c8f4f3cac66e" ) );
#endif
    }

    {
        constexpr auto const N = sha3_512::block_size;

        constexpr char const buf1[] = "";
        constexpr unsigned char buf2[ N - 1 ] = {};
        constexpr unsigned char buf3[ N ] = {};
        constexpr unsigned char buf4[ N + 1 ] = {};

        TEST_EQ( test<sha3_512>( 0, buf1 ), digest_from_hex( "a69f73cca23a9ac5c8b567dc185a756e97c982164fe25859e0d1dcc1475c80a615b2123af1f5f94c11e3e9402c3ac558f500199d95b6d3e301758586281dcd26" ) );
        TEST_EQ( test<sha3_512>( 0, buf2 ), digest_from_hex( "cd87417194c917561a59c7f2eb4b95145971e32e8e4ef3b23b0f190bfd29e3692cc7975275750a27df95d5c6a99b7a341e1b8a38a750a51aca5b77bae41fbbfc" ) );
        TEST_EQ( test<sha3_512>( 0, buf3 ), digest_from_hex( "f8d76fdd8a082a67eaab47b5518ac486cb9a90dcb9f3c9efcfd86d5c8b3f1831601d3c8435f84b9e56da91283d5b98040e6e7b2c8dd9aa5bd4ebdf1823a7cf29" ) );
        TEST_EQ( test<sha3_512>( 0, buf4 ), digest_from_hex( "4ed8ba5741d94caef309c190bc13d18eb0f16942ebea76dcf0c6db1a35311fc04611313ea7d0ff2228a131cd68a84b3872c93d75700601107b6addeaffaa7a90" ) );

        TEST_EQ( test<sha3_512>( 7, buf1 ), digest_from_hex( "f1536509c1233999f06de145520cc1af9060ae6170d0f1d93f53b9fbe6b9a97361418c0543c025d6617f4d2efdc8bf2ad6c2876ca9c4185df07930475901868c" ) );
        TEST_EQ( test<sha3_512>( 7, buf2 ), digest_from_hex( "8829e5266a464eb6b8134ca6c86ff20d9e21d1e8ad7ced228774d61074d7b41f5efe7ee4ffbf2018db2517e23de72e579714f7225795232a6e75869c3e412588" ) );
#if !BOOST_WORKAROUND(BOOST_MSVC, < 1920)
        TEST_EQ( test<sha3_512>( 7, buf3 ), digest_from_hex( "777df6099f998508c5001a3eb50aec9087130969c932febd0cdbaa74e40c5cc3fe2eb9473692c5859dbfd3432b6bbf9d9062a23670ae6a575dd8e7154f22d83a" ) );
        TEST_EQ( test<sha3_512>( 7, buf4 ), digest_from_hex( "be1fcd3011172808c97416cc4fe3bdd030378a82ad916744d19eadec2cf5f4344b779ae4c97808190cb7a968add2a8d151cdba42bc3c9ac2296233bc7515e881" ) );
#endif
    }

    return boost::report_errors();
}
