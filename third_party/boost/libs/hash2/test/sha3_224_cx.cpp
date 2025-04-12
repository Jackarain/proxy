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
        constexpr char const str2[] = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
        constexpr char const str3[] = "\xff";
        constexpr char const str4[] = "\xe5\xe0\x99\x24";

        TEST_EQ( test<sha3_224>( 0, str1 ), digest_from_hex( "e642824c3f8cf24ad09234ee7d3c766fc9a3a5168d0c94ad73b46fdf" ) );
        TEST_EQ( test<sha3_224>( 0, str2 ), digest_from_hex( "8a24108b154ada21c9fd5574494479ba5c7e7ab76ef264ead0fcce33" ) );
        TEST_EQ( test<sha3_224>( 0, str3 ), digest_from_hex( "624edc8a3c0c9d42bc224f0bf37040483432d7a1aeb68935e80f1e1c" ) );
        TEST_EQ( test<sha3_224>( 0, str4 ), digest_from_hex( "a2af03379fbe66d6a16b4235f124516bea38e5e5a83faa0d7175b7b7" ) );

        TEST_EQ( test<sha3_224>( 7, str1 ), digest_from_hex( "9f2238232485861ebf24446304640fd1c226446539ebc832c57b6bfe" ) );
        TEST_EQ( test<sha3_224>( 7, str2 ), digest_from_hex( "3f8b95bf36fce11994f95a54e4b33e4b29fd3f65e650b56b15c1020d" ) );
        TEST_EQ( test<sha3_224>( 7, str3 ), digest_from_hex( "305229297f49870a79c8a0b519b00876dae029387a66e07551d2c618" ) );
        TEST_EQ( test<sha3_224>( 7, str4 ), digest_from_hex( "3180968feb3e6ba4f0d1b9e5d8d77a3c09b7402a32a2b3ec1748eaeb" ) );
    }

    {
        constexpr auto const N = sha3_224::block_size;

        constexpr char const buf1[] = "";
        constexpr unsigned char buf2[ N - 1 ] = {};
        constexpr unsigned char buf3[ N ] = {};
        constexpr unsigned char buf4[ N + 1 ] = {};

        TEST_EQ( test<sha3_224>( 0, buf1 ), digest_from_hex( "6b4e03423667dbb73b6e15454f0eb1abd4597f9a1b078e3f5b5a6bc7" ) );
        TEST_EQ( test<sha3_224>( 0, buf2 ), digest_from_hex( "b6b709fdb9852b8c7439a33595d42dba2940f44c10c3ce09f8b6a87a" ) );
        TEST_EQ( test<sha3_224>( 0, buf3 ), digest_from_hex( "f2b8486fceee2c6a11a604ce4efe217da854829c2c2dcc9a23758b4d" ) );
        TEST_EQ( test<sha3_224>( 0, buf4 ), digest_from_hex( "361ca40de495e03bd091694f1d1372851105046a1bf6d59fafe7d668" ) );

        TEST_EQ( test<sha3_224>( 7, buf1 ), digest_from_hex( "9efd7f9eef5d3ba1e211e916b4ae4cfcf3f5d961069779673c23ef00" ) );
        TEST_EQ( test<sha3_224>( 7, buf2 ), digest_from_hex( "75e7e927f7426794f12e9e8e6dd6ea14d1c97eccea7e758090d26d56" ) );
#if !BOOST_WORKAROUND(BOOST_MSVC, < 1920)
        TEST_EQ( test<sha3_224>( 7, buf3 ), digest_from_hex( "6204402e2b7315449ad467de22613b6c873eb283299028634b50c8c1" ) );
        TEST_EQ( test<sha3_224>( 7, buf4 ), digest_from_hex( "b18c71acab0556cfb7e39eab66042b5bd0849f9c4b5501eb338fe8ef" ) );
#endif
    }

    return boost::report_errors();
}
