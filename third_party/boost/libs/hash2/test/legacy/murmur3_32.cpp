// Copyright 2017, 2018 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/legacy/murmur3.hpp>
#include <boost/core/lightweight_test.hpp>
#include <cstring>
#include <cstddef>

template<class H, class S, std::size_t N> typename H::result_type hash( char const (&s)[ N ], S seed )
{
    H h( seed );

    h.update( s, N-1 );

    return h.result();
}

int main()
{
    // Test vectors from https://stackoverflow.com/questions/14747343/murmurhash3-test-vectors

    BOOST_TEST_EQ( hash<boost::hash2::murmur3_32>( "", 0 ), 0 );
    BOOST_TEST_EQ( hash<boost::hash2::murmur3_32>( "", 1 ), 0x514E28B7u );
    BOOST_TEST_EQ( hash<boost::hash2::murmur3_32>( "", 0xFFFFFFFFu ), 0x81F16F39u );

    BOOST_TEST_EQ( hash<boost::hash2::murmur3_32>( "\xFF\xFF\xFF\xFF", 0 ), 0x76293B50u );
    BOOST_TEST_EQ( hash<boost::hash2::murmur3_32>( "\x21\x43\x65\x87", 0 ), 0xF55B516Bu );
    BOOST_TEST_EQ( hash<boost::hash2::murmur3_32>( "\x21\x43\x65\x87", 0x5082EDEEu ), 0x2362F9DEu );
    BOOST_TEST_EQ( hash<boost::hash2::murmur3_32>( "\x21\x43\x65", 0 ), 0x7E4A8634u );
    BOOST_TEST_EQ( hash<boost::hash2::murmur3_32>( "\x21\x43", 0 ), 0xA0F7B07Au );
    BOOST_TEST_EQ( hash<boost::hash2::murmur3_32>( "\x21", 0 ), 0x72661CF4u );

    BOOST_TEST_EQ( hash<boost::hash2::murmur3_32>( "\x00\x00\x00\x00", 0 ), 0x2362F9DEu );
    BOOST_TEST_EQ( hash<boost::hash2::murmur3_32>( "\x00\x00\x00", 0 ), 0x85F0B427u );
    BOOST_TEST_EQ( hash<boost::hash2::murmur3_32>( "\x00\x00", 0 ), 0x30F4C306u );
    BOOST_TEST_EQ( hash<boost::hash2::murmur3_32>( "\x00", 0 ), 0x514E28B7u );

    BOOST_TEST_EQ( hash<boost::hash2::murmur3_32>( "abc", 0 ), 0xB3DD93FAu );
    BOOST_TEST_EQ( hash<boost::hash2::murmur3_32>( "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 0 ), 0xEE925B90u );
    BOOST_TEST_EQ( hash<boost::hash2::murmur3_32>( "The quick brown fox jumps over the lazy dog", 0x9747B28Cu ), 0x2FA826CDu );

    return boost::report_errors();
}
