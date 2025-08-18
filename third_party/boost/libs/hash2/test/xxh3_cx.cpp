// Copyright 2025 Christian Mazakas
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/digest.hpp>
#include <boost/hash2/xxh3.hpp>
#include <boost/core/lightweight_test.hpp>
#include <cstdint>

#include <boost/config.hpp>

#if defined(BOOST_MSVC) && BOOST_MSVC < 1920
# pragma warning(disable: 4307) // integral constant overflow
#endif

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

#if defined(BOOST_NO_CXX14_CONSTEXPR)
# define TEST_EQ(x1, x2) BOOST_TEST_EQ(x1, x2)
# define TEST_NE(x1, x2) BOOST_TEST_NE(x1, x2)
#else
# define TEST_EQ(x1, x2) BOOST_TEST_EQ(x1, x2); STATIC_ASSERT( x1 == x2 )
# define TEST_NE(x1, x2) BOOST_TEST_NE(x1, x2); STATIC_ASSERT( x1 != x2 )
#endif

constexpr static unsigned char const test_bytes[] =
{
    0, 82, 146, 155, 183, 50, 163, 36, 45, 0, 175, 149, 14, 236, 184, 147, 227, 223, 239, 147, 170, 214, 205, 42, 83, 139, 92, 63, 84, 90, 111, 213, 89,
    192, 255, 252, 143, 133, 185, 51, 29, 171, 116, 247, 182, 5, 147, 39, 176, 112, 132, 179, 103, 124, 159, 118, 72, 0, 114, 237, 123, 152, 23, 232, 221,
    72, 94, 12, 12, 203, 208, 101, 63, 173, 178, 143, 17, 176, 108, 232, 141, 176, 241, 134, 8, 97, 89, 86, 108, 142, 78, 120, 19, 99, 189, 171, 157,
    50, 115, 9, 234, 113, 47, 217, 122, 157, 85, 240, 202, 138, 208, 233, 94, 26, 54, 179, 107, 15, 202, 81, 239, 139, 162, 196, 98, 237, 0, 150, 243,
    52, 73, 235, 15, 209, 59, 146, 161, 169, 99, 219, 170, 237, 61, 207, 241, 9, 66, 205, 249, 179, 33, 162, 235, 242, 200, 244, 228, 47, 72, 209, 75,
    16, 244, 194, 239, 236, 248, 74, 181, 56, 116, 195, 164, 166, 98, 14, 191, 253, 99, 55, 65, 227, 134, 152, 26, 235, 76, 186, 86, 3, 102, 135, 237,
    0, 69, 89, 193, 133, 68, 182, 195, 104, 249, 65, 169, 234, 249, 135, 224, 159, 18, 208, 213, 20, 84, 72, 93, 68, 64, 81, 227, 56, 6, 154, 76,
    60, 13, 239, 100, 137, 248, 163, 97, 238, 227, 197, 28, 147, 104, 200, 229, 151, 215, 9, 152, 255, 151, 168, 76, 234, 164, 121, 171, 205, 44, 200, 159,
    27, 242, 241, 206, 226, 130, 80, 56, 130, 173, 142, 155, 184, 180, 136, 129, 142, 182, 54, 217, 169, 101, 72, 116, 60, 201, 219, 252, 187, 216, 42, 34,
    158, 180, 36, 226, 178, 103, 171, 170, 19, 226, 42, 77, 252, 113, 28, 7, 98, 145, 189, 248, 226, 151, 93, 12, 132, 253, 103, 69, 103, 27, 9, 154,
    75, 32, 162, 119, 48, 149, 218, 40, 177, 134, 63, 158, 242, 225, 219, 16, 223, 144, 248, 109, 145, 47, 6, 120, 179, 72, 91, 223, 246, 219, 216, 124,
    167, 174, 165, 137, 64, 76, 2, 115, 32, 240, 159, 221, 99, 121, 149, 177, 3, 196, 126, 111, 47, 224, 34, 246, 212, 136, 233, 102, 110, 157, 53, 25,
    189, 65, 27, 79, 26, 207, 213, 120, 176, 0, 73, 240, 31, 143, 141, 246, 20, 188, 181, 148, 109, 196, 83, 118, 167, 167, 83, 145, 207, 141, 182, 22,
    108, 57, 126, 54, 47, 6, 242, 35, 30, 142, 15, 200, 134, 244, 118, 97, 142, 246, 100, 54, 252, 140, 39, 161, 250, 18, 203, 254, 206, 88, 63, 129,
    103, 136, 108, 125, 192, 129, 231, 248, 192, 142, 84, 14, 201, 217, 81, 157, 33, 144, 53, 105, 10, 11, 172, 161, 113, 68, 75, 159, 69, 254, 157, 152,
    31, 30, 27, 119, 108, 93, 241, 251, 126, 44, 100, 254, 107, 87, 25, 104, 205, 221, 228, 208, 9, 54, 26, 96, 153, 98, 215, 7, 230, 77, 9,
};

struct xxh3_128_t
{
    std::uint64_t low;
    std::uint64_t high;
};

BOOST_CXX14_CONSTEXPR static boost::hash2::digest<16> hex_encode( xxh3_128_t x )
{
    boost::hash2::digest<16> digest;

    boost::hash2::detail::write64be( digest.data() + 0 , x.high );
    boost::hash2::detail::write64be( digest.data() + 8 , x.low );

    return digest;
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

BOOST_CXX14_CONSTEXPR unsigned char to_byte( char c )
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0xff;
}

template<class H> BOOST_CXX14_CONSTEXPR typename H::result_type hash_with_seed( std::size_t n, std::uint64_t seed )
{
    H h( seed );

    std::size_t m = n / 3;

    h.update( test_bytes, m );
    h.update( test_bytes + m, n - m );

    auto d = h.result();

    H h2 = H::with_seed( seed );
    h2.update( test_bytes, n );
    if( h2.result() != d )
    {
        return {};
    }

    return d;
}

template<class H> BOOST_CXX14_CONSTEXPR typename H::result_type hash_with_secret( std::size_t n, unsigned char const* secret, std::size_t secret_len )
{
    H h = H::with_secret( secret, secret_len );

    std::size_t m = n / 3;

    h.update( test_bytes, m );
    h.update( test_bytes + m, n - m );

    return h.result();
}

template<class H> BOOST_CXX14_CONSTEXPR typename H::result_type hash_with_secret_and_seed( std::size_t n, unsigned char const* secret, std::size_t secret_len, std::uint64_t seed )
{
    H h = H::with_secret_and_seed( secret, secret_len, seed );

    std::size_t m = n / 3;

    h.update( test_bytes, m );
    h.update( test_bytes + m, n - m );

    return h.result();
}

int main()
{
    using namespace boost::hash2;

    // empty

    TEST_EQ( hash_with_seed<xxh3_128>( 0, 0x0000000000000000ull ), hex_encode( { 0x6001c324468d497full, 0x99aa06d3014798d8ull } ) );

    TEST_EQ( hash_with_seed<xxh3_128>( 0, 0x000000009e3779b1ull ), hex_encode( { 0x5444f7869c671ab0ull, 0x92220ae55e14ab50ull }  ) );
    TEST_EQ( hash_with_seed<xxh3_128>( 0, 0x9e3779b185ebca8dull ), hex_encode( { 0xa986dfc5d7605bfeull, 0x00feaa732a3ce25eull } ) );

    // 1-to-3

    TEST_EQ( hash_with_seed<xxh3_128>( 1, 0x0000000000000000ull ), hex_encode( { 0xc44bdff4074eecdbull, 0xa6cd5e9392000f6aull } ) );
    TEST_EQ( hash_with_seed<xxh3_128>( 1, 0x000000009e3779b1ull ), hex_encode( { 0xb53d5557e7f76f8dull, 0x89b99554ba22467cull } ) );
    TEST_EQ( hash_with_seed<xxh3_128>( 1, 0x9e3779b185ebca8dull ), hex_encode( { 0x032be332dd766ef8ull, 0x20e49abcc53b3842ull } ) );

    TEST_EQ( hash_with_seed<xxh3_128>( 3, 0x0000000000000000ull ), hex_encode( { 0x54247382a8d6b94dull, 0x20efc49ff02422eaull } ) );
    TEST_EQ( hash_with_seed<xxh3_128>( 3, 0x000000009e3779b1ull ), hex_encode( { 0xf173d14dad53a5dcull, 0x48f82c2fe0abd468ull } ) );
    TEST_EQ( hash_with_seed<xxh3_128>( 3, 0x9e3779b185ebca8dull ), hex_encode( { 0x634b8990b4976373ull, 0x1c7ecf6a308cf00eull } ) );

    // 4-to-8

    TEST_EQ( hash_with_seed<xxh3_128>( 4, 0x0000000000000000ull ), hex_encode( { 0x2e7d8d6876a39fe9ull, 0x970d585ac632bf8eull } ) );
    TEST_EQ( hash_with_seed<xxh3_128>( 4, 0x000000009e3779b1ull ), hex_encode( { 0xef78d5c489cfe10bull, 0x7170492a2aa08992ull } ) );
    TEST_EQ( hash_with_seed<xxh3_128>( 4, 0x9e3779b185ebca8dull ), hex_encode( { 0xbfaf51f1e67e0b0full, 0x3d53e5dfd837d927ull } ) );

    TEST_EQ( hash_with_seed<xxh3_128>( 8, 0x0000000000000000ull ), hex_encode( { 0x64c69cab4bb21dc5ull, 0x47a7f080d82bb456ull } ) );
    TEST_EQ( hash_with_seed<xxh3_128>( 8, 0x000000009e3779b1ull ), hex_encode( { 0x5f462f3de2e8b940ull, 0xf959013232655ff1ull } ) );
    TEST_EQ( hash_with_seed<xxh3_128>( 8, 0x9e3779b185ebca8dull ), hex_encode( { 0x7b29471dc729b5ffull, 0xf50cec145bcd5c5aull } ) );

    // 9-to-16

    TEST_EQ( hash_with_seed<xxh3_128>( 9, 0x0000000000000000ull ), hex_encode( { 0xed7ccbc501eb7501ull, 0x564ef6078950d457ull } ) );
    TEST_EQ( hash_with_seed<xxh3_128>( 9, 0x000000009e3779b1ull ), hex_encode( { 0x07de00b45eee033aull, 0x75fb6d1bd353b45cull } ) );
    TEST_EQ( hash_with_seed<xxh3_128>( 9, 0x9e3779b185ebca8dull ), hex_encode( { 0xaef5dfc0ac9f9044ull, 0x6b380b43ffa61042ull } ) );

    TEST_EQ( hash_with_seed<xxh3_128>( 16, 0x0000000000000000ull ), hex_encode( { 0x562980258a998629ull, 0xc68c368ecf8a9c05ull } ) );
    TEST_EQ( hash_with_seed<xxh3_128>( 16, 0x000000009e3779b1ull ), hex_encode( { 0xb07eeeab4c56392bull, 0x3767c90d0cdbb93dull } ) );
    TEST_EQ( hash_with_seed<xxh3_128>( 16, 0x9e3779b185ebca8dull ), hex_encode( { 0x0346d13a7a5498c7ull, 0x6ffcb80cd33085c8ull } ) );

    // 17-to-128

    TEST_EQ( hash_with_seed<xxh3_128>( 17, 0x0000000000000000ull ), hex_encode( { 0xabbc12d11973d7dbull, 0x955fa78643ed3669ull } ) );
    TEST_EQ( hash_with_seed<xxh3_128>( 17, 0x000000009e3779b1ull ), hex_encode( { 0x3cc9ff6cae79accbull, 0x99e7c628e75d6431ull } ) );
    TEST_EQ( hash_with_seed<xxh3_128>( 17, 0x9e3779b185ebca8dull ), hex_encode( { 0x980a14119985a7dfull, 0xd77681219e464828ull } ) );

    TEST_EQ( hash_with_seed<xxh3_128>( 128, 0x0000000000000000ull ), hex_encode( { 0xebb15e34a7fb5ab1ull, 0x39992220e045260aull } ) );
    TEST_EQ( hash_with_seed<xxh3_128>( 128, 0x000000009e3779b1ull ), hex_encode( { 0x1453819941d93c1dull, 0x98801187df8d614dull } ) );
    TEST_EQ( hash_with_seed<xxh3_128>( 128, 0x9e3779b185ebca8dull ), hex_encode( { 0x8394f5c51f1d8246ull, 0xa0f7ccb68ee02addull } ) );

    // 129-to-240

    TEST_EQ( hash_with_seed<xxh3_128>( 129, 0x0000000000000000ull ), hex_encode( { 0x86c9e3bc8f0a3b5cull, 0x03815fc91f1b30b6ull } ) );
    TEST_EQ( hash_with_seed<xxh3_128>( 129, 0x000000009e3779b1ull ), hex_encode( { 0xb37b716f66b40f02ull, 0xb7f7349a47b39e56ull } ) );
    TEST_EQ( hash_with_seed<xxh3_128>( 129, 0x9e3779b185ebca8dull ), hex_encode( { 0xd4aae26fcec7dc03ull, 0xad559266067c0bf3ull } ) );

    TEST_EQ( hash_with_seed<xxh3_128>( 240, 0x0000000000000000ull ), hex_encode( { 0x5c9aae94c8ebe5a0ull, 0xaa4202daa2769dc8ull } ) );
    TEST_EQ( hash_with_seed<xxh3_128>( 240, 0x000000009e3779b1ull ), hex_encode( { 0xca19087f1d335daeull, 0xda888104beae5ae0ull } ) );
    TEST_EQ( hash_with_seed<xxh3_128>( 240, 0x9e3779b185ebca8dull ), hex_encode( { 0x604e98db085c1864ull, 0x29d2133d6ea58c5bull } ) );

    // 240+

    TEST_EQ( hash_with_seed<xxh3_128>( 241, 0x0000000000000000ull ), hex_encode( { 0xc5a639ecd2030e5eull, 0x99a80ecf0ecfc647ull } ) );
    TEST_EQ( hash_with_seed<xxh3_128>( 241, 0x000000009e3779b1ull ), hex_encode( { 0x5927e3637bac8149ull, 0x4bf2229c3a8fc3c3ull } ) );
    TEST_EQ( hash_with_seed<xxh3_128>( 241, 0x9e3779b185ebca8dull ), hex_encode( { 0xdda9b0a161d4829aull, 0xec64afae6a137582ull } ) );

    TEST_EQ( hash_with_seed<xxh3_128>( 255, 0x0000000000000000ull ), hex_encode( { 0xe98f979f4ed8a197ull, 0x961375c87e09efbcull } ) );
    TEST_EQ( hash_with_seed<xxh3_128>( 255, 0x000000009e3779b1ull ), hex_encode( { 0x437ea109cb7ce24dull, 0xee657e12607adffeull } ) );
    TEST_EQ( hash_with_seed<xxh3_128>( 255, 0x9e3779b185ebca8dull ), hex_encode( { 0x2aca7901d9538c75ull, 0xe72ec0137d62df44ull } ) );

    TEST_EQ( hash_with_seed<xxh3_128>( 256, 0x0000000000000000ull ), hex_encode( { 0x55de574ad89d0ac5ull, 0x8b1c66091423d288ull } ) );
    TEST_EQ( hash_with_seed<xxh3_128>( 256, 0x000000009e3779b1ull ), hex_encode( { 0x443d04d43f60c57full, 0xd540cc8620d8dd65ull } ) );
    TEST_EQ( hash_with_seed<xxh3_128>( 256, 0x9e3779b185ebca8dull ), hex_encode( { 0x4d30234b7a3aa61cull, 0xaaa57235b92d5e7cull } ) );

    TEST_EQ( hash_with_seed<xxh3_128>( 257, 0x0000000000000000ull ), hex_encode( { 0xb17fd5a8ae75bb0bull, 0xf15fee7f9f457599ull } ) );
    TEST_EQ( hash_with_seed<xxh3_128>( 257, 0x000000009e3779b1ull ), hex_encode( { 0x02f16a1476c65d95ull, 0x52c36ca232fc662bull } ) );
    TEST_EQ( hash_with_seed<xxh3_128>( 257, 0x9e3779b185ebca8dull ), hex_encode( { 0x802a6fbf3cacd97cull, 0x15c1f9c667c815baull } ) );

    TEST_EQ( hash_with_seed<xxh3_128>( 511, 0x0000000000000000ull ), hex_encode( { 0x8089715b163e7fc0ull, 0x9f7619cb8d250f0dull } ) );
    TEST_EQ( hash_with_seed<xxh3_128>( 511, 0x000000009e3779b1ull ), hex_encode( { 0x96736274a52c7db2ull, 0x24e3bb97c7c584d4ull } ) );
    TEST_EQ( hash_with_seed<xxh3_128>( 511, 0x9e3779b185ebca8dull ), hex_encode( { 0x90ec0377ba8d6002ull, 0xb52cae55536e9fb9ull } ) );

    TEST_EQ( hash_with_seed<xxh3_128>( 512, 0x0000000000000000ull ), hex_encode( { 0x617e49599013cb6bull, 0x18d2d110dcc9bca1ull } ) );
    TEST_EQ( hash_with_seed<xxh3_128>( 512, 0x000000009e3779b1ull ), hex_encode( { 0x545f610e9f5a78ecull, 0x06eeb0d56508040full } ) );
    TEST_EQ( hash_with_seed<xxh3_128>( 512, 0x9e3779b185ebca8dull ), hex_encode( { 0x3ce457de14c27708ull, 0x925d06b8ec5b8040ull } ) );

    constexpr unsigned char const* secret = test_bytes + 7;
    constexpr std::size_t const secret_len = 136 + 11;

    TEST_EQ( hash_with_secret<xxh3_128>(    0, secret, secret_len ), hex_encode( { 0x005923cceecbe8aeull, 0x5f70f4ea232f1d38ull } ) );
    TEST_EQ( hash_with_secret<xxh3_128>(    1, secret, secret_len ), hex_encode( { 0x8a52451418b2da4dull, 0x3a66af5a9819198eull } ) );
    TEST_EQ( hash_with_secret<xxh3_128>(    3, secret, secret_len ), hex_encode( { 0xe9af94712ffbc846ull, 0x51103173fa1f0727ull } ) );
    TEST_EQ( hash_with_secret<xxh3_128>(    4, secret, secret_len ), hex_encode( { 0x266a9b610a7a5641ull, 0xccc924914b0d8032ull } ) );
    TEST_EQ( hash_with_secret<xxh3_128>(    8, secret, secret_len ), hex_encode( { 0xf668474d2fee1f92ull, 0x20ed43ff46f7a0a1ull } ) );
    TEST_EQ( hash_with_secret<xxh3_128>(    9, secret, secret_len ), hex_encode( { 0xc3bbf94649c59dfcull, 0x6af09813af70cfd1ull } ) );
    TEST_EQ( hash_with_secret<xxh3_128>(   16, secret, secret_len ), hex_encode( { 0xfe396195466852b9ull, 0x4c317fd601bcda88ull } ) );
    TEST_EQ( hash_with_secret<xxh3_128>(   17, secret, secret_len ), hex_encode( { 0xe94eb4616009b975ull, 0x604cc5ee8f142950ull } ) );
    TEST_EQ( hash_with_secret<xxh3_128>(  128, secret, secret_len ), hex_encode( { 0xb8feec0b6b6eaf60ull, 0x1df8cce15fe35b2cull } ) );
    TEST_EQ( hash_with_secret<xxh3_128>(  129, secret, secret_len ), hex_encode( { 0x9def70d87b89ed7bull, 0x72d4d4395002b150ull } ) );
    TEST_EQ( hash_with_secret<xxh3_128>(  240, secret, secret_len ), hex_encode( { 0x29dd17317e40cba2ull, 0x8033fd83d4336ca9ull } ) );
    TEST_EQ( hash_with_secret<xxh3_128>(  241, secret, secret_len ), hex_encode( { 0x454805371df98a91ull, 0x0ecde988107f17f2ull } ) );
    TEST_EQ( hash_with_secret<xxh3_128>(  255, secret, secret_len ), hex_encode( { 0xe1e3461712968b3eull, 0xf44f7290a7123665ull } ) );
    TEST_EQ( hash_with_secret<xxh3_128>(  256, secret, secret_len ), hex_encode( { 0xd4cba59e2e2cf9f0ull, 0xdc8cd5dc03c0da95ull } ) );
    TEST_EQ( hash_with_secret<xxh3_128>(  257, secret, secret_len ), hex_encode( { 0x1e4b71e703d08492ull, 0x15fda9442e840f61ull } ) );
    TEST_EQ( hash_with_secret<xxh3_128>(  511, secret, secret_len ), hex_encode( { 0x13e7046bc1c1f16aull, 0x86764f81bb226a35ull } ) );
    TEST_EQ( hash_with_secret<xxh3_128>(  512, secret, secret_len ), hex_encode( { 0x7564693dd526e28dull, 0x918c0f2c7656ab6dull } ) );

    constexpr digest<16> d = {};

    TEST_NE( hash_with_secret<xxh3_128>( 0, secret,   1 ), d );
    TEST_NE( hash_with_secret<xxh3_128>( 0, secret, 135 ), d );

    constexpr std::uint64_t const seed = 0x9e3779b185ebca8dull;

    TEST_EQ( hash_with_secret_and_seed<xxh3_128>(   0, secret, secret_len, seed ), hex_encode( { 0xa986dfc5d7605bfeull, 0x00feaa732a3ce25eull } ) );
    TEST_EQ( hash_with_secret_and_seed<xxh3_128>(   1, secret, secret_len, seed ), hex_encode( { 0x032be332dd766ef8ull, 0x20e49abcc53b3842ull } ) );
    TEST_EQ( hash_with_secret_and_seed<xxh3_128>(   3, secret, secret_len, seed ), hex_encode( { 0x634b8990b4976373ull, 0x1c7ecf6a308cf00eull } ) );
    TEST_EQ( hash_with_secret_and_seed<xxh3_128>(   4, secret, secret_len, seed ), hex_encode( { 0xbfaf51f1e67e0b0full, 0x3d53e5dfd837d927ull } ) );
    TEST_EQ( hash_with_secret_and_seed<xxh3_128>(   8, secret, secret_len, seed ), hex_encode( { 0x7b29471dc729b5ffull, 0xf50cec145bcd5c5aull } ) );
    TEST_EQ( hash_with_secret_and_seed<xxh3_128>(   9, secret, secret_len, seed ), hex_encode( { 0xaef5dfc0ac9f9044ull, 0x6b380b43ffa61042ull } ) );
    TEST_EQ( hash_with_secret_and_seed<xxh3_128>(  16, secret, secret_len, seed ), hex_encode( { 0x0346d13a7a5498c7ull, 0x6ffcb80cd33085c8ull } ) );
    TEST_EQ( hash_with_secret_and_seed<xxh3_128>(  17, secret, secret_len, seed ), hex_encode( { 0x980a14119985a7dfull, 0xd77681219e464828ull } ) );
    TEST_EQ( hash_with_secret_and_seed<xxh3_128>( 128, secret, secret_len, seed ), hex_encode( { 0x8394f5c51f1d8246ull, 0xa0f7ccb68ee02addull } ) );
    TEST_EQ( hash_with_secret_and_seed<xxh3_128>( 129, secret, secret_len, seed ), hex_encode( { 0xd4aae26fcec7dc03ull, 0xad559266067c0bf3ull } ) );
    TEST_EQ( hash_with_secret_and_seed<xxh3_128>( 240, secret, secret_len, seed ), hex_encode( { 0x604e98db085c1864ull, 0x29d2133d6ea58c5bull } ) );
    TEST_EQ( hash_with_secret_and_seed<xxh3_128>( 241, secret, secret_len, seed ), hex_encode( { 0x454805371df98a91ull, 0x0ecde988107f17f2ull } ) );
    TEST_EQ( hash_with_secret_and_seed<xxh3_128>( 255, secret, secret_len, seed ), hex_encode( { 0xe1e3461712968b3eull, 0xf44f7290a7123665ull } ) );
    TEST_EQ( hash_with_secret_and_seed<xxh3_128>( 256, secret, secret_len, seed ), hex_encode( { 0xd4cba59e2e2cf9f0ull, 0xdc8cd5dc03c0da95ull } ) );
    TEST_EQ( hash_with_secret_and_seed<xxh3_128>( 257, secret, secret_len, seed ), hex_encode( { 0x1e4b71e703d08492ull, 0x15fda9442e840f61ull } ) );
    TEST_EQ( hash_with_secret_and_seed<xxh3_128>( 511, secret, secret_len, seed ), hex_encode( { 0x13e7046bc1c1f16aull, 0x86764f81bb226a35ull } ) );
    TEST_EQ( hash_with_secret_and_seed<xxh3_128>( 512, secret, secret_len, seed ), hex_encode( { 0x7564693dd526e28dull, 0x918c0f2c7656ab6dull } ) );

    return boost::report_errors();
}
