// Copyright 2025 Christian Mazakas.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/xxh3.hpp>
#include <boost/core/lightweight_test.hpp>
#include <cstring>
#include <vector>

static std::vector<unsigned char> make_test_bytes( std::size_t n )
{
    std::vector<unsigned char> v( n, 0 );

    std::uint64_t gen = 2654435761u;
    for( std::size_t i = 0; i < n; ++i )
    {
        v[ i ] = static_cast<unsigned char>( gen >> 56 );
        gen *= 11400714785074694797ull;
    }

    return v;
}

template<class H, class S> typename H::result_type hash_with_seed( char const * s, S seed )
{
    std::size_t n = std::strlen( s );

    H h( seed );

    h.update( s, n );

    auto d = h.result();

    H h2 = H::with_seed( seed );

    std::size_t m = n / 3;

    h2.update( s, m );
    h2.update( s + m, n - m );
    BOOST_TEST_EQ( to_string( d ), to_string( h2.result() ) );

    return d;

}

template<class H, class S> typename H::result_type hash_with_seed( std::vector<unsigned char> const& s, std::size_t n, S seed )
{
    H h( seed );

    h.update( s.data(), n );
    auto d = h.result();

    H h2 = H::with_seed( seed );

    std::size_t m = n / 3;

    h2.update( s.data(), m );
    h2.update( s.data() + m, n - m );
    BOOST_TEST_EQ( to_string( d ), to_string( h2.result() ) );

    return d;
}

template<class H> typename H::result_type hash_with_secret( std::vector<unsigned char> const& s, std::size_t n, unsigned char const* secret, std::size_t secret_len )
{
    H h = H::with_secret( secret, secret_len );

    h.update( s.data(), n );
    auto d = h.result();

    H h2 = H::with_secret( secret, secret_len );

    std::size_t m = n / 3;

    h2.update( s.data(), m );
    h2.update( s.data() + m, n - m );
    BOOST_TEST_EQ( to_string( d ), to_string( h2.result() ) );

    return d;
}

template<class H> typename H::result_type hash_with_secret_and_seed( std::vector<unsigned char> const& s, std::size_t n, unsigned char const* secret, std::size_t secret_len, std::uint64_t seed )
{
    H h = H::with_secret_and_seed( secret, secret_len, seed );

    h.update( s.data(), n );
    auto d = h.result();

    H h2 = H::with_secret_and_seed( secret, secret_len, seed );

    std::size_t m = n / 3;

    h2.update( s.data(), m );
    h2.update( s.data() + m, n - m );
    BOOST_TEST_EQ( to_string( d ), to_string( h2.result() ) );

    return d;
}

struct xxh3_128_t
{
    std::uint64_t low;
    std::uint64_t high;
};

static std::string hex_encode( xxh3_128_t x )
{
    boost::hash2::digest<16> digest;

    boost::hash2::detail::write64be( digest.data() + 0 , x.high );
    boost::hash2::detail::write64be( digest.data() + 8 , x.low );

    return to_string( digest );
}

static void test_xxh3_128()
{
    using namespace boost::hash2;

    // Test vectors from https://raw.githubusercontent.com/Cyan4973/xxHash/refs/heads/dev/tests/sanity_test_vectors.h

    auto const v = make_test_bytes( 4096 + 64 + 1 );

    // empty

    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( "", 0 ) ), std::string( "99aa06d3014798d86001c324468d497f" ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( "", 0 ) ), hex_encode( { 0x6001c324468d497full, 0x99aa06d3014798d8ull } ) );

    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( "", 0x000000009e3779b1ull ) ), hex_encode( { 0x5444f7869c671ab0ull, 0x92220ae55e14ab50ull }  ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( "", 0x9e3779b185ebca8dull ) ), hex_encode( { 0xa986dfc5d7605bfeull, 0x00feaa732a3ce25eull } ) );

    // 1-to-3

    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 1, 0x0000000000000000ull ) ), hex_encode( { 0xc44bdff4074eecdbull, 0xa6cd5e9392000f6aull } ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 1, 0x000000009e3779b1ull ) ), hex_encode( { 0xb53d5557e7f76f8dull, 0x89b99554ba22467cull } ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 1, 0x9e3779b185ebca8dull ) ), hex_encode( { 0x032be332dd766ef8ull, 0x20e49abcc53b3842ull } ) );

    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 3, 0x0000000000000000ull ) ), hex_encode( { 0x54247382a8d6b94dull, 0x20efc49ff02422eaull } ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 3, 0x000000009e3779b1ull ) ), hex_encode( { 0xf173d14dad53a5dcull, 0x48f82c2fe0abd468ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 3, 0x9e3779b185ebca8dull ) ), hex_encode( { 0x634b8990b4976373ull, 0x1c7ecf6a308cf00eull } ) );

    // 4-to-8

    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 4, 0x0000000000000000ull ) ), hex_encode( { 0x2e7d8d6876a39fe9ull, 0x970d585ac632bf8eull } ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 4, 0x000000009e3779b1ull ) ), hex_encode( { 0xef78d5c489cfe10bull, 0x7170492a2aa08992ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 4, 0x9e3779b185ebca8dull ) ), hex_encode( { 0xbfaf51f1e67e0b0full, 0x3d53e5dfd837d927ull } ) );

    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 8, 0x0000000000000000ull ) ), hex_encode( { 0x64c69cab4bb21dc5ull, 0x47a7f080d82bb456ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 8, 0x000000009e3779b1ull ) ), hex_encode( { 0x5f462f3de2e8b940ull, 0xf959013232655ff1ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 8, 0x9e3779b185ebca8dull ) ), hex_encode( { 0x7b29471dc729b5ffull, 0xf50cec145bcd5c5aull } ) );

    // 9-to-16

    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 9, 0x0000000000000000ull ) ), hex_encode( { 0xed7ccbc501eb7501ull, 0x564ef6078950d457ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 9, 0x000000009e3779b1ull ) ), hex_encode( { 0x07de00b45eee033aull, 0x75fb6d1bd353b45cull } ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 9, 0x9e3779b185ebca8dull ) ), hex_encode( { 0xaef5dfc0ac9f9044ull, 0x6b380b43ffa61042ull } ) );

    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 16, 0x0000000000000000ull ) ), hex_encode( { 0x562980258a998629ull, 0xc68c368ecf8a9c05ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 16, 0x000000009e3779b1ull ) ), hex_encode( { 0xb07eeeab4c56392bull, 0x3767c90d0cdbb93dull } ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 16, 0x9e3779b185ebca8dull ) ), hex_encode( { 0x0346d13a7a5498c7ull, 0x6ffcb80cd33085c8ull } ) );

    // 17-to-128

    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 17, 0x0000000000000000ull ) ), hex_encode( { 0xabbc12d11973d7dbull, 0x955fa78643ed3669ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 17, 0x000000009e3779b1ull ) ), hex_encode( { 0x3cc9ff6cae79accbull, 0x99e7c628e75d6431ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 17, 0x9e3779b185ebca8dull ) ), hex_encode( { 0x980a14119985a7dfull, 0xd77681219e464828ull } ) );

    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 128, 0x0000000000000000ull ) ), hex_encode( { 0xebb15e34a7fb5ab1ull, 0x39992220e045260aull } ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 128, 0x000000009e3779b1ull ) ), hex_encode( { 0x1453819941d93c1dull, 0x98801187df8d614dull } ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 128, 0x9e3779b185ebca8dull ) ), hex_encode( { 0x8394f5c51f1d8246ull, 0xa0f7ccb68ee02addull } ) );

    // 129-to-240

    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 129, 0x0000000000000000ull ) ), hex_encode( { 0x86c9e3bc8f0a3b5cull, 0x03815fc91f1b30b6ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 129, 0x000000009e3779b1ull ) ), hex_encode( { 0xb37b716f66b40f02ull, 0xb7f7349a47b39e56ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 129, 0x9e3779b185ebca8dull ) ), hex_encode( { 0xd4aae26fcec7dc03ull, 0xad559266067c0bf3ull } ) );

    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 240, 0x0000000000000000ull ) ), hex_encode( { 0x5c9aae94c8ebe5a0ull, 0xaa4202daa2769dc8ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 240, 0x000000009e3779b1ull ) ), hex_encode( { 0xca19087f1d335daeull, 0xda888104beae5ae0ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 240, 0x9e3779b185ebca8dull ) ), hex_encode( { 0x604e98db085c1864ull, 0x29d2133d6ea58c5bull } ) );

    // 240+

    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 241, 0x0000000000000000ull ) ), hex_encode( { 0xc5a639ecd2030e5eull, 0x99a80ecf0ecfc647ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 241, 0x000000009e3779b1ull ) ), hex_encode( { 0x5927e3637bac8149ull, 0x4bf2229c3a8fc3c3ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 241, 0x9e3779b185ebca8dull ) ), hex_encode( { 0xdda9b0a161d4829aull, 0xec64afae6a137582ull } ) );

    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 255, 0x0000000000000000ull ) ), hex_encode( { 0xe98f979f4ed8a197ull, 0x961375c87e09efbcull } ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 255, 0x000000009e3779b1ull ) ), hex_encode( { 0x437ea109cb7ce24dull, 0xee657e12607adffeull } ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 255, 0x9e3779b185ebca8dull ) ), hex_encode( { 0x2aca7901d9538c75ull, 0xe72ec0137d62df44ull } ) );

    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 256, 0x0000000000000000ull ) ), hex_encode( { 0x55de574ad89d0ac5ull, 0x8b1c66091423d288ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 256, 0x000000009e3779b1ull ) ), hex_encode( { 0x443d04d43f60c57full, 0xd540cc8620d8dd65ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 256, 0x9e3779b185ebca8dull ) ), hex_encode( { 0x4d30234b7a3aa61cull, 0xaaa57235b92d5e7cull } ) );

    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 257, 0x0000000000000000ull ) ), hex_encode( { 0xb17fd5a8ae75bb0bull, 0xf15fee7f9f457599ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 257, 0x000000009e3779b1ull ) ), hex_encode( { 0x02f16a1476c65d95ull, 0x52c36ca232fc662bull } ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 257, 0x9e3779b185ebca8dull ) ), hex_encode( { 0x802a6fbf3cacd97cull, 0x15c1f9c667c815baull } ) );

    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 511, 0x0000000000000000ull ) ), hex_encode( { 0x8089715b163e7fc0ull, 0x9f7619cb8d250f0dull } ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 511, 0x000000009e3779b1ull ) ), hex_encode( { 0x96736274a52c7db2ull, 0x24e3bb97c7c584d4ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 511, 0x9e3779b185ebca8dull ) ), hex_encode( { 0x90ec0377ba8d6002ull, 0xb52cae55536e9fb9ull } ) );

    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 512, 0x0000000000000000ull ) ), hex_encode( { 0x617e49599013cb6bull, 0x18d2d110dcc9bca1ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 512, 0x000000009e3779b1ull ) ), hex_encode( { 0x545f610e9f5a78ecull, 0x06eeb0d56508040full } ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 512, 0x9e3779b185ebca8dull ) ), hex_encode( { 0x3ce457de14c27708ull, 0x925d06b8ec5b8040ull } ) );

    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 1023, 0x0000000000000000ull ) ), hex_encode( { 0x87a8f7b2f2e22496ull, 0xe8083e4d83214c3cull } ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 1023, 0x000000009e3779b1ull ) ), hex_encode( { 0xc38922d5971cd2d7ull, 0xfbd299789b9a9759ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 1023, 0x9e3779b185ebca8dull ) ), hex_encode( { 0x0f0f02de8590e1b5ull, 0x96b80fe329ce5e35ull } ) );

    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 1024, 0x0000000000000000ull ) ), hex_encode( { 0xdd85c9b5c1109c5cull, 0x0d30d24071c64c57ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 1024, 0x000000009e3779b1ull ) ), hex_encode( { 0xb8b95c07cd4a75faull, 0x885b0b4debe3d2ffull } ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 1024, 0x9e3779b185ebca8dull ) ), hex_encode( { 0xef368a8a2ebabaefull, 0x17600efe2b493a18ull } ) );

    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 1025, 0x0000000000000000ull ) ), hex_encode( { 0xd870c0fa13211c6aull, 0xfd3ee4fe7f2954c6ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 1025, 0x000000009e3779b1ull ) ), hex_encode( { 0x2f15255340ae4f6cull, 0x3364fad6f5ff1741ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 1025, 0x9e3779b185ebca8dull ) ), hex_encode( { 0x96792bcf9af88519ull, 0x2c383949f57bf7e1ull } ) );

    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 2047, 0x0000000000000000ull ) ), hex_encode( { 0xb36ece19fca2197full, 0x763a9143f0523d15ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 2047, 0x000000009e3779b1ull ) ), hex_encode( { 0x8141f69f4bacdea2ull, 0xd2605592ab25dc1aull } ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 2047, 0x9e3779b185ebca8dull ) ), hex_encode( { 0x8111bb82842ed0aeull, 0x47c992688e710651ull } ) );

    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 2048, 0x0000000000000000ull ) ), hex_encode( { 0xdd59e2c3a5f038e0ull, 0xf736557fd47073a5ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 2048, 0x000000009e3779b1ull ) ), hex_encode( { 0x230d43f30206260bull, 0x7fb03f7e7186c3eaull } ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 2048, 0x9e3779b185ebca8dull ) ), hex_encode( { 0x66f81670669ababcull, 0x23cc3a2e75ebaaeaull } ) );

    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 2049, 0x0000000000000000ull ) ), hex_encode( { 0xd3afa4329779b921ull, 0x4cd2bd192f2d70bdull } ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 2049, 0x000000009e3779b1ull ) ), hex_encode( { 0x60e0f49946d79dafull, 0x782bc6262b86bfe8ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_seed<xxh3_128>( v, 2049, 0x9e3779b185ebca8dull ) ), hex_encode( { 0xe48083836cd58024ull, 0xe4000f7a288a82ceull } ) );

    // test with custom secret

    unsigned char const* secret = v.data() + 7;
    std::size_t const secret_len = 136 + 11;

    BOOST_TEST_EQ( to_string( hash_with_secret<xxh3_128>( v,    0, secret, secret_len ) ), hex_encode( { 0x005923cceecbe8aeull, 0x5f70f4ea232f1d38ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_secret<xxh3_128>( v,    1, secret, secret_len ) ), hex_encode( { 0x8a52451418b2da4dull, 0x3a66af5a9819198eull } ) );
    BOOST_TEST_EQ( to_string( hash_with_secret<xxh3_128>( v,    3, secret, secret_len ) ), hex_encode( { 0xe9af94712ffbc846ull, 0x51103173fa1f0727ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_secret<xxh3_128>( v,    4, secret, secret_len ) ), hex_encode( { 0x266a9b610a7a5641ull, 0xccc924914b0d8032ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_secret<xxh3_128>( v,    8, secret, secret_len ) ), hex_encode( { 0xf668474d2fee1f92ull, 0x20ed43ff46f7a0a1ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_secret<xxh3_128>( v,    9, secret, secret_len ) ), hex_encode( { 0xc3bbf94649c59dfcull, 0x6af09813af70cfd1ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_secret<xxh3_128>( v,   16, secret, secret_len ) ), hex_encode( { 0xfe396195466852b9ull, 0x4c317fd601bcda88ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_secret<xxh3_128>( v,   17, secret, secret_len ) ), hex_encode( { 0xe94eb4616009b975ull, 0x604cc5ee8f142950ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_secret<xxh3_128>( v,  128, secret, secret_len ) ), hex_encode( { 0xb8feec0b6b6eaf60ull, 0x1df8cce15fe35b2cull } ) );
    BOOST_TEST_EQ( to_string( hash_with_secret<xxh3_128>( v,  129, secret, secret_len ) ), hex_encode( { 0x9def70d87b89ed7bull, 0x72d4d4395002b150ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_secret<xxh3_128>( v,  240, secret, secret_len ) ), hex_encode( { 0x29dd17317e40cba2ull, 0x8033fd83d4336ca9ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_secret<xxh3_128>( v,  241, secret, secret_len ) ), hex_encode( { 0x454805371df98a91ull, 0x0ecde988107f17f2ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_secret<xxh3_128>( v,  255, secret, secret_len ) ), hex_encode( { 0xe1e3461712968b3eull, 0xf44f7290a7123665ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_secret<xxh3_128>( v,  256, secret, secret_len ) ), hex_encode( { 0xd4cba59e2e2cf9f0ull, 0xdc8cd5dc03c0da95ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_secret<xxh3_128>( v,  257, secret, secret_len ) ), hex_encode( { 0x1e4b71e703d08492ull, 0x15fda9442e840f61ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_secret<xxh3_128>( v,  511, secret, secret_len ) ), hex_encode( { 0x13e7046bc1c1f16aull, 0x86764f81bb226a35ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_secret<xxh3_128>( v,  512, secret, secret_len ) ), hex_encode( { 0x7564693dd526e28dull, 0x918c0f2c7656ab6dull } ) );
    BOOST_TEST_EQ( to_string( hash_with_secret<xxh3_128>( v, 1023, secret, secret_len ) ), hex_encode( { 0x6df5a1773b876cfbull, 0x21fe7c4fbcebe042ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_secret<xxh3_128>( v, 1024, secret, secret_len ) ), hex_encode( { 0x3538a2d1ea7410d0ull, 0x7663338d0b32666dull } ) );
    BOOST_TEST_EQ( to_string( hash_with_secret<xxh3_128>( v, 1025, secret, secret_len ) ), hex_encode( { 0xe33739f32d405604ull, 0x3644184c7d1e8f29ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_secret<xxh3_128>( v, 2047, secret, secret_len ) ), hex_encode( { 0x209243520dbdb300ull, 0x47aa10ba88a049f3ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_secret<xxh3_128>( v, 2048, secret, secret_len ) ), hex_encode( { 0xd32e975821d6519full, 0xe862d841c07049afull } ) );
    BOOST_TEST_EQ( to_string( hash_with_secret<xxh3_128>( v, 2049, secret, secret_len ) ), hex_encode( { 0xa21be3a04630def3ull, 0x545e67046af902fbull } ) );

    // not testing the output, just making sure we don't segfault here

    BOOST_TEST_NE( to_string( hash_with_secret<xxh3_128>( v, 0, secret,   1 ) ), std::string( "" ));
    BOOST_TEST_NE( to_string( hash_with_secret<xxh3_128>( v, 0, secret, 135 ) ), std::string( "" ));

    // test with seed and custom secret

    std::uint64_t const seed = 0x9e3779b185ebca8dull;

    BOOST_TEST_EQ( to_string( hash_with_secret_and_seed<xxh3_128>( v,    0, secret, secret_len, seed ) ), hex_encode( { 0xa986dfc5d7605bfeull, 0x00feaa732a3ce25eull } ) );
    BOOST_TEST_EQ( to_string( hash_with_secret_and_seed<xxh3_128>( v,    1, secret, secret_len, seed ) ), hex_encode( { 0x032be332dd766ef8ull, 0x20e49abcc53b3842ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_secret_and_seed<xxh3_128>( v,    3, secret, secret_len, seed ) ), hex_encode( { 0x634b8990b4976373ull, 0x1c7ecf6a308cf00eull } ) );
    BOOST_TEST_EQ( to_string( hash_with_secret_and_seed<xxh3_128>( v,    4, secret, secret_len, seed ) ), hex_encode( { 0xbfaf51f1e67e0b0full, 0x3d53e5dfd837d927ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_secret_and_seed<xxh3_128>( v,    8, secret, secret_len, seed ) ), hex_encode( { 0x7b29471dc729b5ffull, 0xf50cec145bcd5c5aull } ) );
    BOOST_TEST_EQ( to_string( hash_with_secret_and_seed<xxh3_128>( v,    9, secret, secret_len, seed ) ), hex_encode( { 0xaef5dfc0ac9f9044ull, 0x6b380b43ffa61042ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_secret_and_seed<xxh3_128>( v,   16, secret, secret_len, seed ) ), hex_encode( { 0x0346d13a7a5498c7ull, 0x6ffcb80cd33085c8ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_secret_and_seed<xxh3_128>( v,   17, secret, secret_len, seed ) ), hex_encode( { 0x980a14119985a7dfull, 0xd77681219e464828ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_secret_and_seed<xxh3_128>( v,  128, secret, secret_len, seed ) ), hex_encode( { 0x8394f5c51f1d8246ull, 0xa0f7ccb68ee02addull } ) );
    BOOST_TEST_EQ( to_string( hash_with_secret_and_seed<xxh3_128>( v,  129, secret, secret_len, seed ) ), hex_encode( { 0xd4aae26fcec7dc03ull, 0xad559266067c0bf3ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_secret_and_seed<xxh3_128>( v,  240, secret, secret_len, seed ) ), hex_encode( { 0x604e98db085c1864ull, 0x29d2133d6ea58c5bull } ) );
    BOOST_TEST_EQ( to_string( hash_with_secret_and_seed<xxh3_128>( v,  241, secret, secret_len, seed ) ), hex_encode( { 0x454805371df98a91ull, 0x0ecde988107f17f2ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_secret_and_seed<xxh3_128>( v,  255, secret, secret_len, seed ) ), hex_encode( { 0xe1e3461712968b3eull, 0xf44f7290a7123665ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_secret_and_seed<xxh3_128>( v,  256, secret, secret_len, seed ) ), hex_encode( { 0xd4cba59e2e2cf9f0ull, 0xdc8cd5dc03c0da95ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_secret_and_seed<xxh3_128>( v,  257, secret, secret_len, seed ) ), hex_encode( { 0x1e4b71e703d08492ull, 0x15fda9442e840f61ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_secret_and_seed<xxh3_128>( v,  511, secret, secret_len, seed ) ), hex_encode( { 0x13e7046bc1c1f16aull, 0x86764f81bb226a35ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_secret_and_seed<xxh3_128>( v,  512, secret, secret_len, seed ) ), hex_encode( { 0x7564693dd526e28dull, 0x918c0f2c7656ab6dull } ) );
    BOOST_TEST_EQ( to_string( hash_with_secret_and_seed<xxh3_128>( v, 1023, secret, secret_len, seed ) ), hex_encode( { 0x6df5a1773b876cfbull, 0x21fe7c4fbcebe042ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_secret_and_seed<xxh3_128>( v, 1024, secret, secret_len, seed ) ), hex_encode( { 0x3538a2d1ea7410d0ull, 0x7663338d0b32666dull } ) );
    BOOST_TEST_EQ( to_string( hash_with_secret_and_seed<xxh3_128>( v, 1025, secret, secret_len, seed ) ), hex_encode( { 0xe33739f32d405604ull, 0x3644184c7d1e8f29ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_secret_and_seed<xxh3_128>( v, 2047, secret, secret_len, seed ) ), hex_encode( { 0x209243520dbdb300ull, 0x47aa10ba88a049f3ull } ) );
    BOOST_TEST_EQ( to_string( hash_with_secret_and_seed<xxh3_128>( v, 2048, secret, secret_len, seed ) ), hex_encode( { 0xd32e975821d6519full, 0xe862d841c07049afull } ) );
    BOOST_TEST_EQ( to_string( hash_with_secret_and_seed<xxh3_128>( v, 2049, secret, secret_len, seed ) ), hex_encode( { 0xa21be3a04630def3ull, 0x545e67046af902fbull } ) );
}

int main()
{
    test_xxh3_128();

    return boost::report_errors();
}
