// Copyright 2024 Christian Mazakas
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/sha3.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <boost/config/workaround.hpp>
#include <array>

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

# define TEST_EQ(x1, x2) BOOST_TEST_EQ(x1, x2); STATIC_ASSERT(x1 == x2)

#endif

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

template<std::size_t D, std::size_t N>
BOOST_CXX14_CONSTEXPR boost::hash2::digest<D> truncate( boost::hash2::digest<N> const& digest )
{
    boost::hash2::digest<D> s;
    boost::hash2::detail::memcpy( s.data(), digest.data(), s.size() );
    return s;
}

int main()
{
    using namespace boost::hash2;

    BOOST_CXX14_CONSTEXPR digest<32> r1 = digest_from_hex( "a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a" );
    BOOST_CXX14_CONSTEXPR digest<28> r2 = digest_from_hex( "6b4e03423667dbb73b6e15454f0eb1abd4597f9a1b078e3f5b5a6bc7" );
    BOOST_CXX14_CONSTEXPR digest<64> r3 = digest_from_hex( "a69f73cca23a9ac5c8b567dc185a756e97c982164fe25859e0d1dcc1475c80a615b2123af1f5f94c11e3e9402c3ac558f500199d95b6d3e301758586281dcd26" );
    BOOST_CXX14_CONSTEXPR digest<48> r4 = digest_from_hex( "0c63a75b845e4f7d01107d852e4c2485c51a50aaaa94fc61995e71bbee983a2ac3713831264adb47fb6bd1e058d5f004" );
    BOOST_CXX14_CONSTEXPR digest<32> r5 = digest_from_hex( "7f9c2ba4e88f827d616045507605853ed73b8093f6efbc88eb1a6eacfa66ef26" );
    BOOST_CXX14_CONSTEXPR digest<64> r6 = digest_from_hex( "46b9dd2b0ba88d13233b3feb743eeb243fcd52ea62b81b82b50c27646ed5762fd75dc4ddd8c0f200cb05019d67b592f6fc821c49479ab48640292eacb3b7c4be" );

    TEST_EQ( sha3_256().result(), r1 );
    TEST_EQ( sha3_256(0).result(), r1 );
    TEST_EQ( sha3_256(static_cast<unsigned char const*>(nullptr), 0).result(), r1 );

    TEST_EQ( sha3_224().result(), r2 );
    TEST_EQ( sha3_224(0).result(), r2 );
    TEST_EQ( sha3_224(static_cast<unsigned char const*>(nullptr), 0).result(), r2 );

    TEST_EQ( sha3_512().result(), r3 );
    TEST_EQ( sha3_512(0).result(), r3 );
    TEST_EQ( sha3_512(static_cast<unsigned char const*>(nullptr), 0).result(), r3 );

    TEST_EQ( sha3_384().result(), r4 );
    TEST_EQ( sha3_384(0).result(), r4 );
    TEST_EQ( sha3_384(static_cast<unsigned char const*>(nullptr), 0).result(), r4 );

    TEST_EQ( truncate<32>( shake_128().result() ), r5 );
    TEST_EQ( truncate<32>( shake_128(0).result() ), r5 );
    TEST_EQ( truncate<32>( shake_128(static_cast<unsigned char const*>(nullptr), 0).result() ), r5 );

    TEST_EQ( truncate<64>( shake_256().result() ), r6 );
    TEST_EQ( truncate<64>( shake_256(0).result() ), r6 );
    TEST_EQ( truncate<64>( shake_256(static_cast<unsigned char const*>(nullptr), 0).result() ), r6 );

    return boost::report_errors();
}
