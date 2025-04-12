// Copyright 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/xxhash.hpp>
#include <boost/core/lightweight_test.hpp>
#include <vector>
#include <cstdint>

constexpr auto PRIME32 = 2654435761U;
constexpr auto PRIME64 = 11400714785074694797ULL;

template<class H> typename H::result_type test( std::size_t n, std::uint64_t seed )
{
    std::vector<unsigned char> buffer( n );

    std::uint64_t w = PRIME32;

    for( std::size_t i = 0; i < n; ++i)
    {
        buffer[ i ] = static_cast<unsigned char>( w >> 56 );
        w *= PRIME64;
    }

    H h( seed );
    h.update( buffer.data(), buffer.size() );

    return h.result();
}

int main()
{
    // Test vectors from https://github.com/Cyan4973/xxHash/blob/dd11140c2dc5d53a3c0a949d67af7f40f546878e/cli/xsum_sanity_check.c

    BOOST_TEST_EQ( test<boost::hash2::xxhash_32>( 0, 0 ), 0x02CC5D05U );
    BOOST_TEST_EQ( test<boost::hash2::xxhash_32>( 0, PRIME32 ), 0x36B78AE7U );
    BOOST_TEST_EQ( test<boost::hash2::xxhash_32>( 1, 0 ), 0xCF65B03EU );
    BOOST_TEST_EQ( test<boost::hash2::xxhash_32>( 1, PRIME32 ), 0xB4545AA4U );
    BOOST_TEST_EQ( test<boost::hash2::xxhash_32>( 14, 0 ), 0x1208E7E2U );
    BOOST_TEST_EQ( test<boost::hash2::xxhash_32>( 14, PRIME32 ), 0x6AF1D1FEU );
    BOOST_TEST_EQ( test<boost::hash2::xxhash_32>( 222, 0 ), 0x5BD11DBDU );
    BOOST_TEST_EQ( test<boost::hash2::xxhash_32>( 222, PRIME32 ), 0x58803C5FU );

    BOOST_TEST_EQ( test<boost::hash2::xxhash_64>( 0, 0 ), 0xEF46DB3751D8E999ULL );
    BOOST_TEST_EQ( test<boost::hash2::xxhash_64>( 0, PRIME32 ), 0xAC75FDA2929B17EFULL );
    BOOST_TEST_EQ( test<boost::hash2::xxhash_64>( 1, 0 ), 0xE934A84ADB052768ULL );
    BOOST_TEST_EQ( test<boost::hash2::xxhash_64>( 1, PRIME32 ), 0x5014607643A9B4C3ULL );
    BOOST_TEST_EQ( test<boost::hash2::xxhash_64>( 4, 0 ), 0x9136A0DCA57457EEULL );
    BOOST_TEST_EQ( test<boost::hash2::xxhash_64>( 14, 0 ), 0x8282DCC4994E35C8ULL );
    BOOST_TEST_EQ( test<boost::hash2::xxhash_64>( 14, PRIME32 ), 0xC3BD6BF63DEB6DF0ULL );
    BOOST_TEST_EQ( test<boost::hash2::xxhash_64>( 222, 0 ), 0xB641AE8CB691C174ULL );
    BOOST_TEST_EQ( test<boost::hash2::xxhash_64>( 222, PRIME32 ), 0x20CB8AB7AE10C14AULL );

    return boost::report_errors();
}
