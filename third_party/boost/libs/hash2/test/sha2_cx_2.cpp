// Copyright 2024 Christian Mazakas
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/sha2.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <array>

#if defined(BOOST_MSVC) && BOOST_MSVC < 1920
# pragma warning(disable: 4307) // integral constant overflow
#endif

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

#if defined(BOOST_NO_CXX14_CONSTEXPR)

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

int main()
{
    using namespace boost::hash2;

    BOOST_CXX14_CONSTEXPR digest<32> r1 = digest_from_hex( "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" );
    BOOST_CXX14_CONSTEXPR digest<28> r2 = digest_from_hex( "d14a028c2a3a2bc9476102bb288234c415a2b01f828ea62ac5b3e42f" );
    BOOST_CXX14_CONSTEXPR digest<64> r3 = digest_from_hex( "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e" );
    BOOST_CXX14_CONSTEXPR digest<48> r4 = digest_from_hex( "38b060a751ac96384cd9327eb1b1e36a21fdb71114be07434c0cc7bf63f6e1da274edebfe76f65fbd51ad2f14898b95b" );
    BOOST_CXX14_CONSTEXPR digest<28> r5 = digest_from_hex( "6ed0dd02806fa89e25de060c19d3ac86cabb87d6a0ddd05c333b84f4" );
    BOOST_CXX14_CONSTEXPR digest<32> r6 = digest_from_hex( "c672b8d1ef56ed28ab87c3622c5114069bdd3ad7b8f9737498d0c01ecef0967a" );

    TEST_EQ( sha2_256().result(), r1 );
    TEST_EQ( sha2_256(0).result(), r1 );
    TEST_EQ( sha2_256(static_cast<unsigned char const*>(nullptr), 0).result(), r1 );

    TEST_EQ( sha2_224().result(), r2 );
    TEST_EQ( sha2_224(0).result(), r2 );
    TEST_EQ( sha2_224(static_cast<unsigned char const*>(nullptr), 0).result(), r2 );

    TEST_EQ( sha2_512().result(), r3 );
    TEST_EQ( sha2_512(0).result(), r3 );
    TEST_EQ( sha2_512(static_cast<unsigned char const*>(nullptr), 0).result(), r3 );

    TEST_EQ( sha2_384().result(), r4 );
    TEST_EQ( sha2_384(0).result(), r4 );
    TEST_EQ( sha2_384(static_cast<unsigned char const*>(nullptr), 0).result(), r4 );

    TEST_EQ( sha2_512_224().result(), r5 );
    TEST_EQ( sha2_512_224(0).result(), r5 );
    TEST_EQ( sha2_512_224(static_cast<unsigned char const*>(nullptr), 0).result(), r5 );

    TEST_EQ( sha2_512_256().result(), r6 );
    TEST_EQ( sha2_512_256(0).result(), r6 );
    TEST_EQ( sha2_512_256(static_cast<unsigned char const*>(nullptr), 0).result(), r6 );

    return boost::report_errors();
}
