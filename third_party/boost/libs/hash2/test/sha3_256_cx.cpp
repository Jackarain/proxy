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
        constexpr char const str3[] = "\xbd";
        constexpr char const str4[] = "\xc9\x8c\x8e\x55";

        TEST_EQ( test<sha3_256>( 0, str1 ), digest_from_hex( "3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532" ) );
        TEST_EQ( test<sha3_256>( 0, str2 ), digest_from_hex( "41c0dba2a9d6240849100376a8235e2c82e1b9998a999e21db32dd97496d3376" ) );
        TEST_EQ( test<sha3_256>( 0, str3 ), digest_from_hex( "b389fa0f45f21196cc2736e8de396497a2414be31e7a500a499918b8cf3257b2" ) );
        TEST_EQ( test<sha3_256>( 0, str4 ), digest_from_hex( "faad1cd75a6947e88d210123959b29d8f0973b8d094582debe56742878f412f6" ) );

        TEST_EQ( test<sha3_256>( 7, str1 ), digest_from_hex( "767b76e08c084e1ceb87f67e0ec20d02892a7842d91091dbc141d73bab54ee3b" ) );
        TEST_EQ( test<sha3_256>( 7, str2 ), digest_from_hex( "6e62b382e3a6d91715f5c488e1318196d0bc4c875f8bc7229c717aa4fa31f5ae" ) );
        TEST_EQ( test<sha3_256>( 7, str3 ), digest_from_hex( "c1c987cdc6fd9dff69ca8bbd39f8ff5afc412545f046232c4e97402d6c94a2bc" ) );
        TEST_EQ( test<sha3_256>( 7, str4 ), digest_from_hex( "a0eb0aeef201cf512ac37017810b8aae4d6f71f40e376195dcbf2848cb743d01" ) );
    }

    {
        constexpr auto const N = sha3_256::block_size;

        constexpr char const buf1[] = "";
        constexpr unsigned char buf2[ N - 1 ] = {};
        constexpr unsigned char buf3[ N ] = {};
        constexpr unsigned char buf4[ N + 1 ] = {};

        TEST_EQ( test<sha3_256>( 0, buf1 ), digest_from_hex( "a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a" ) );
        TEST_EQ( test<sha3_256>( 0, buf2 ), digest_from_hex( "7d080d7ba978a75c8a7d1f9be566c859084509c9c2b4928435c225d5777d98e3" ) );
        TEST_EQ( test<sha3_256>( 0, buf3 ), digest_from_hex( "e772c9cf9eb9c991cdfcf125001b454fdbc0a95f188d1b4c844aa032ad6e075e" ) );
        TEST_EQ( test<sha3_256>( 0, buf4 ), digest_from_hex( "9ed57188470a83b758cd71c00c6cc3beb984b36a6c35864b4e53017b24cf5699" ) );

        TEST_EQ( test<sha3_256>( 7, buf1 ), digest_from_hex( "aeaead8936dca916412308cc89e143fd7abee742b63e58af03edbc70df57c189" ) );
        TEST_EQ( test<sha3_256>( 7, buf2 ), digest_from_hex( "62a22f45158da5a7793a0760320a74d5b37b4acb92ed97dc2fef39c97ede5bf3" ) );
#if !BOOST_WORKAROUND(BOOST_MSVC, < 1920)
        TEST_EQ( test<sha3_256>( 7, buf3 ), digest_from_hex( "06e557dd0972ee7a39e587a844f67545aefa2368b08806b68f5b3d0315323a42" ) );
        TEST_EQ( test<sha3_256>( 7, buf4 ), digest_from_hex( "85bb3a4d934cbf99ebe0e798969d9cef3a50b456b37a1f2e2c96c71f913cb254" ) );
#endif
    }

    return boost::report_errors();
}
