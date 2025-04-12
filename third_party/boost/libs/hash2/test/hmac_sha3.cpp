// Copyright 2024 Christian Mazakas.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#define _CRT_SECURE_NO_WARNINGS

#ifdef _MSC_VER
# pragma warning(disable: 4309) // truncation of constant value
#endif

#include <boost/hash2/sha3.hpp>
#include <boost/core/lightweight_test.hpp>
#include <tuple>
#include <vector>

std::string from_hex( char const* str )
{
    auto f = []( char c ) { return ( c >= 'a' ? c - 'a' + 10 : c - '0' ); };

    std::string s;
    while( *str != '\0' )
    {
        s.push_back( static_cast<char>( ( f( str[ 0 ] ) << 4 ) + f( str[ 1 ] ) ) );
        str += 2;
    }
    return s;
}

template<class H> std::string digest( std::string const & k, std::string const & s )
{
    H h( k.data(), k.size() );

    h.update( s.data(), s.size() );

    return to_string( h.result() );
}

static void hmac_sha3_224()
{
    using boost::hash2::hmac_sha3_224;

    // https://csrc.nist.gov/projects/cryptographic-standards-and-guidelines/example-values
    // https://csrc.nist.gov/CSRC/media/Projects/Cryptographic-Standards-and-Guidelines/documents/examples/HMAC_SHA3-224.pdf
    // note: must use `std::make_tuple()` for gcc-5, gcc-4.8 because of https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/n4387.html
    std::vector<std::tuple<char const*, char const*, char const*>> inputs =
    {                    /* expected mac */                                          /* key */                                                   /* msg */
        std::make_tuple( "332cfd59347fdb8e576e77260be4aba2d6dc53117b3bfb52c6d18c04", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b", "53616d706c65206d65737361676520666f72206b65796c656e3c626c6f636b6c656e" ),
        std::make_tuple( "d8b733bcf66c644a12323d564e24dcf3fc75f231f3b67968359100c7", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f808182838485868788898a8b8c8d8e8f", "53616d706c65206d65737361676520666f72206b65796c656e3d626c6f636b6c656e"),
        std::make_tuple( "078695eecc227c636ad31d063a15dd05a7e819a66ec6d8de1e193e59", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9fa0a1a2a3a4a5a6a7a8a9aaab", "53616d706c65206d65737361676520666f72206b65796c656e3e626c6f636b6c656e" ),
        std::make_tuple( "8569c54cbb00a9b78ff1b391b0e5cd2fa5ec728550aa3979703305d4", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b", "53616d706c65206d65737361676520666f72206b65796c656e3c626c6f636b6c656e2c2077697468207472756e636174656420746167" ),
    };

    for( auto const& input : inputs )
    {
        BOOST_TEST_EQ( digest<hmac_sha3_224>( from_hex( std::get<1>( input ) ), from_hex( std::get<2>( input) ) ), std::string( std::get<0>( input ) ) );
    }
}

static void hmac_sha3_256()
{
    using boost::hash2::hmac_sha3_256;

    // https://csrc.nist.gov/projects/cryptographic-standards-and-guidelines/example-values
    // https://csrc.nist.gov/CSRC/media/Projects/Cryptographic-Standards-and-Guidelines/documents/examples/HMAC_SHA3-256.pdf
    // note: must use `std::make_tuple()` for gcc-5, gcc-4.8 because of https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/n4387.html
    std::vector<std::tuple<char const*, char const*, char const*>> inputs =
    {                    /* expected mac */                                                  /* key */                                                           /* msg */
        std::make_tuple( "4fe8e202c4f058e8dddc23d8c34e467343e23555e24fc2f025d598f558f67205", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f", "53616d706c65206d65737361676520666f72206b65796c656e3c626c6f636b6c656e" ),
        std::make_tuple( "68b94e2e538a9be4103bebb5aa016d47961d4d1aa906061313b557f8af2c3faa", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f8081828384858687", "53616d706c65206d65737361676520666f72206b65796c656e3d626c6f636b6c656e"),
        std::make_tuple( "9bcf2c238e235c3ce88404e813bd2f3a97185ac6f238c63d6229a00b07974258", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9fa0a1a2a3a4a5a6a7", "53616d706c65206d65737361676520666f72206b65796c656e3e626c6f636b6c656e" ),
        std::make_tuple( "c8dc7148d8c1423aa549105dafdf9cad2941471b5c62207088e56ccf2dd80545", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f", "53616d706c65206d65737361676520666f72206b65796c656e3c626c6f636b6c656e2c2077697468207472756e636174656420746167" ),
    };

    for( auto const& input : inputs )
    {
        BOOST_TEST_EQ( digest<hmac_sha3_256>( from_hex( std::get<1>( input ) ), from_hex( std::get<2>( input) ) ), std::string( std::get<0>( input ) ) );
    }
}

static void hmac_sha3_384()
{
    using boost::hash2::hmac_sha3_384;

    // https://csrc.nist.gov/projects/cryptographic-standards-and-guidelines/example-values
    // https://csrc.nist.gov/CSRC/media/Projects/Cryptographic-Standards-and-Guidelines/documents/examples/HMAC_SHA3-384.pdf
    // note: must use `std::make_tuple()` for gcc-5, gcc-4.8 because of https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/n4387.html
    std::vector<std::tuple<char const*, char const*, char const*>> inputs =
    {                    /* expected mac */                                                                                  /* key */                                                                                           /* msg */
        std::make_tuple( "d588a3c51f3f2d906e8298c1199aa8ff6296218127f6b38a90b6afe2c5617725bc99987f79b22a557b6520db710b7f42", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f", "53616d706c65206d65737361676520666f72206b65796c656e3c626c6f636b6c656e" ),
        std::make_tuple( "a27d24b592e8c8cbf6d4ce6fc5bf62d8fc98bf2d486640d9eb8099e24047837f5f3bffbe92dcce90b4ed5b1e7e44fa90", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f6061626364656667", "53616d706c65206d65737361676520666f72206b65796c656e3d626c6f636b6c656e"),
        std::make_tuple( "e5ae4c739f455279368ebf36d4f5354c95aa184c899d3870e460ebc288ef1f9470053f73f7c6da2a71bcaec38ce7d6ac", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f808182838485868788898a8b8c8d8e8f9091929394959697", "53616d706c65206d65737361676520666f72206b65796c656e3e626c6f636b6c656e" ),
        std::make_tuple( "25f4bf53606e91af79d24a4bb1fd6aecd44414a30c8ebb0ae09764c71aceefe8dfa72309e48152c98294be658a33836e", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f", "53616d706c65206d65737361676520666f72206b65796c656e3c626c6f636b6c656e2c2077697468207472756e636174656420746167" ),
    };

    for( auto const& input : inputs )
    {
        BOOST_TEST_EQ( digest<hmac_sha3_384>( from_hex( std::get<1>( input ) ), from_hex( std::get<2>( input) ) ), std::string( std::get<0>( input ) ) );
    }
}

static void hmac_sha3_512()
{
    using boost::hash2::hmac_sha3_512;

    // https://csrc.nist.gov/projects/cryptographic-standards-and-guidelines/example-values
    // https://csrc.nist.gov/CSRC/media/Projects/Cryptographic-Standards-and-Guidelines/documents/examples/HMAC_SHA3-512.pdf
    // note: must use `std::make_tuple()` for gcc-5, gcc-4.8 because of https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/n4387.html
    std::vector<std::tuple<char const*, char const*, char const*>> inputs =
    {                    /* expected mac */                                                                                                                  /* key */                                                                                                                           /* msg */
        std::make_tuple( "4efd629d6c71bf86162658f29943b1c308ce27cdfa6db0d9c3ce81763f9cbce5f7ebe9868031db1a8f8eb7b6b95e5c5e3f657a8996c86a2f6527e307f0213196", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f", "53616d706c65206d65737361676520666f72206b65796c656e3c626c6f636b6c656e" ),
        std::make_tuple( "544e257ea2a3e5ea19a590e6a24b724ce6327757723fe2751b75bf007d80f6b360744bf1b7a88ea585f9765b47911976d3191cf83c039f5ffab0d29cc9d9b6da", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f4041424344454647", "53616d706c65206d65737361676520666f72206b65796c656e3d626c6f636b6c656e"),
        std::make_tuple( "5f464f5e5b7848e3885e49b2c385f0694985d0e38966242dc4a5fe3fea4b37d46b65ceced5dcf59438dd840bab22269f0ba7febdb9fcf74602a35666b2a32915", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f8081828384858687", "53616d706c65206d65737361676520666f72206b65796c656e3e626c6f636b6c656e" ),
        std::make_tuple( "7bb06d859257b25ce73ca700df34c5cbef5c898bac91029e0b27975d4e526a088f5e590ee736969f445643a58bee7ee0cbbbb2e14775584435d36ad0de6b9499", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f", "53616d706c65206d65737361676520666f72206b65796c656e3c626c6f636b6c656e2c2077697468207472756e636174656420746167" ),
    };

    for( auto const& input : inputs )
    {
        BOOST_TEST_EQ( digest<hmac_sha3_512>( from_hex( std::get<1>( input ) ), from_hex( std::get<2>( input) ) ), std::string( std::get<0>( input ) ) );
    }
}

int main()
{
    hmac_sha3_224();
    hmac_sha3_256();
    hmac_sha3_384();
    hmac_sha3_512();

    return boost::report_errors();
}
