#include <boost/hash2/blake2.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <boost/config/workaround.hpp>

#include <cstdint>

#if defined(BOOST_MSVC) && BOOST_MSVC < 1920
# pragma warning(disable: 4307) // integral constant overflow
#endif

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

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

template<class H, std::size_t KeyLen, std::size_t N> BOOST_CXX14_CONSTEXPR typename H::result_type test_hex( boost::hash2::digest<KeyLen> key, char const (&str)[ N ] )
{
    H h( key.data(), key.size() );

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
        constexpr char const buf1[] = "";
        constexpr char const buf2[] = "00";
        constexpr char const buf3[] = "000102";
        constexpr char const buf4[] = "000102030405060708090a0b";
        constexpr char const buf5[] = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e";
        constexpr char const buf6[] = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f";
        constexpr char const buf7[] = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f80";
        constexpr char const buf8[] = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f808182";
        constexpr char const buf9[] = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f808182838485868788898a";
        constexpr char const buf10[] = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9fa0a1a2a3a4a5a6a7a8a9aaabacadaeafb0b1b2b3b4b5b6b7b8b9babbbcbdbebfc0c1c2c3c4c5c6c7c8c9cacbcccdcecfd0d1d2d3d4d5d6d7d8d9dadbdcdddedfe0e1e2e3e4e5e6e7e8e9eaebecedeeeff0f1f2f3f4f5f6f7f8f9fafbfcfdfe";

        TEST_EQ( test_hex<blake2b_512>( 0, buf1 ), digest_from_hex( "786a02f742015903c6c6fd852552d272912f4740e15847618a86e217f71f5419d25e1031afee585313896444934eb04b903a685b1448b755d56f701afe9be2ce" ) );
        TEST_EQ( test_hex<blake2b_512>( 0, buf2 ), digest_from_hex( "2fa3f686df876995167e7c2e5d74c4c7b6e48f8068fe0e44208344d480f7904c36963e44115fe3eb2a3ac8694c28bcb4f5a0f3276f2e79487d8219057a506e4b" ) );
        TEST_EQ( test_hex<blake2b_512>( 0, buf3 ), digest_from_hex( "40a374727302d9a4769c17b5f409ff32f58aa24ff122d7603e4fda1509e919d4107a52c57570a6d94e50967aea573b11f86f473f537565c66f7039830a85d186" ) );
        TEST_EQ( test_hex<blake2b_512>( 0, buf4 ), digest_from_hex( "10f0dc91b9f845fb95fad6860e6ce1adfa002c7fc327116d44d047cd7d5870d772bb12b5fac00e02b08ac2a0174d0446c36ab35f14ca31894cd61c78c849b48a" ) );
        TEST_EQ( test_hex<blake2b_512>( 0, buf5 ), digest_from_hex( "b6292669ccd38d5f01caae96ba272c76a879a45743afa0725d83b9ebb26665b731f1848c52f11972b6644f554c064fa90780dbbbf3a89d4fc31f67df3e5857ef" ) );
        TEST_EQ( test_hex<blake2b_512>( 0, buf6 ), digest_from_hex( "2319e3789c47e2daa5fe807f61bec2a1a6537fa03f19ff32e87eecbfd64b7e0e8ccff439ac333b040f19b0c4ddd11a61e24ac1fe0f10a039806c5dcc0da3d115" ) );
        TEST_EQ( test_hex<blake2b_512>( 0, buf7 ), digest_from_hex( "f59711d44a031d5f97a9413c065d1e614c417ede998590325f49bad2fd444d3e4418be19aec4e11449ac1a57207898bc57d76a1bcf3566292c20c683a5c4648f" ) );
        TEST_EQ( test_hex<blake2b_512>( 0, buf8 ), digest_from_hex( "a3eb6e6c7bf2fb8b28bfe8b15e15bb500f781ecc86f778c3a4e655fc5869bf2846a245d4e33b7b14436a17e63be79b36655c226a50ffbc7124207b0202342db5" ) );
        TEST_EQ( test_hex<blake2b_512>( 0, buf9 ), digest_from_hex( "d400601f9728ccc4c92342d9787d8d28ab323af375ca5624b4bb91d17271fbae862e413be73f1f68e615b8c5c391be0dbd9144746eb339ad541547ba9c468a17" ) );
        TEST_EQ( test_hex<blake2b_512>( 0, buf10 ), digest_from_hex( "5b21c5fd8868367612474fa2e70e9cfa2201ffeee8fafab5797ad58fefa17c9b5b107da4a3db6320baaf2c8617d5a51df914ae88da3867c2d41f0cc14fa67928" ) );

        // verified against openssl
        TEST_EQ( test_hex<blake2b_512>( 7, buf1 ), digest_from_hex( "fab5f28da3d6f84c66b7d3186d3df8deeeabfad59066d09cd957fe34d99b57572f6de72a3aa7aa39cdeace268a11036adc4bb2dd2b6aaca4898e6c9fca3f8663" ) );
        TEST_EQ( test_hex<blake2b_512>( 7, buf2 ), digest_from_hex( "92c191040fe6b2382a52aadfa77fe9f26cec090c0feb6de0f5427c32236554c3a4d3e148b810031d21f75f4a17fbbb0399ea452094a968441730a312dc70e231" ) );
        TEST_EQ( test_hex<blake2b_512>( 7, buf3 ), digest_from_hex( "204ed93860415f36ebcfeeddbbcc10741683b835d1b5b5a2f104a8cf1ce6e452c3882cfda7bfb0433f611a94ccbb04d0a1e8a8212c82760c589aaf16d87d8eb7" ) );
        TEST_EQ( test_hex<blake2b_512>( 7, buf4 ), digest_from_hex( "65838d536a9fdac01dbf6c2d2891de5b5ec8e1c839f01c9cef12abf70c0944110cc35699f332d7758f231cb533cd38d3d3eca03f02c38854013b4a8052438f62" ) );
        TEST_EQ( test_hex<blake2b_512>( 7, buf5 ), digest_from_hex( "4699b80146e0e43e667ecdfe9fb2862d766d0e713f739ee91fb9d42eafd48be0e0f8077e43821dae2082ea0d5791decc6e0da7b49321b1a2176114e45f4c05f1" ) );
        TEST_EQ( test_hex<blake2b_512>( 7, buf6 ), digest_from_hex( "f6dd94cfa4e040690306dcd989ed1ac3e1b91c2cb3d9d91f343f44618ba59d8820b313537ab22c2cd95722427e1caeb17fd02ccf1ce7f96b8c31a35d060078f0" ) );
        TEST_EQ( test_hex<blake2b_512>( 7, buf7 ), digest_from_hex( "43b07582396a9864f23ad9ba01ab43be69c7636ee75268885ce655385257f6da8dc554085ed9bafb6d50887a98d85e53345500964d865689337c5565024dcf91" ) );
        TEST_EQ( test_hex<blake2b_512>( 7, buf8 ), digest_from_hex( "e0a2d323b89542768d0b0ede40116f7bb16f4d2b4bd28a1e32044076fe112dbfa9d2db98fa52b82b37e5f97bb0ed35eddb9455b03be2f0d50e5822619cf7ac96" ) );
        TEST_EQ( test_hex<blake2b_512>( 7, buf9 ), digest_from_hex( "59f064d384f53bb1dda06154c9e2839dac9fc8409a8313a0f5d1bb28d5d06dc9ef5691928d34455b7f4c1780f3a1e42053ef9239a79cbd4dc874a766ff46a173" ) );
        TEST_EQ( test_hex<blake2b_512>( 7, buf10 ), digest_from_hex( "1926e26e24ab2be209816409fcae9f5f5ae1d4765a3dfeaa7b7eda06f465bdb9cbf9833b62e7966aba7199c7be467af6a9313ca6f6abc1b1e39b918ee117c950" ) );

        BOOST_CXX14_CONSTEXPR auto const key = digest_from_hex( "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f" );

        TEST_EQ( test_hex<blake2b_512>( key, buf1 ), digest_from_hex( "10ebb67700b1868efb4417987acf4690ae9d972fb7a590c2f02871799aaa4786b5e996e8f0f4eb981fc214b005f42d2ff4233499391653df7aefcbc13fc51568" ) );
        TEST_EQ( test_hex<blake2b_512>( key, buf2 ), digest_from_hex( "961f6dd1e4dd30f63901690c512e78e4b45e4742ed197c3c5e45c549fd25f2e4187b0bc9fe30492b16b0d0bc4ef9b0f34c7003fac09a5ef1532e69430234cebd" ) );
        TEST_EQ( test_hex<blake2b_512>( key, buf3 ), digest_from_hex( "33d0825dddf7ada99b0e7e307104ad07ca9cfd9692214f1561356315e784f3e5a17e364ae9dbb14cb2036df932b77f4b292761365fb328de7afdc6d8998f5fc1" ) );
        TEST_EQ( test_hex<blake2b_512>( key, buf4 ), digest_from_hex( "962452a8455cc56c8511317e3b1f3b2c37df75f588e94325fdd77070359cf63a9ae6e930936fdf8e1e08ffca440cfb72c28f06d89a2151d1c46cd5b268ef8563" ) );
        TEST_EQ( test_hex<blake2b_512>( key, buf5 ), digest_from_hex( "76d2d819c92bce55fa8e092ab1bf9b9eab237a25267986cacf2b8ee14d214d730dc9a5aa2d7b596e86a1fd8fa0804c77402d2fcd45083688b218b1cdfa0dcbcb" ) );
        TEST_EQ( test_hex<blake2b_512>( key, buf6 ), digest_from_hex( "72065ee4dd91c2d8509fa1fc28a37c7fc9fa7d5b3f8ad3d0d7a25626b57b1b44788d4caf806290425f9890a3a2a35a905ab4b37acfd0da6e4517b2525c9651e4" ) );
        TEST_EQ( test_hex<blake2b_512>( key, buf7 ), digest_from_hex( "64475dfe7600d7171bea0b394e27c9b00d8e74dd1e416a79473682ad3dfdbb706631558055cfc8a40e07bd015a4540dcdea15883cbbf31412df1de1cd4152b91" ) );
        TEST_EQ( test_hex<blake2b_512>( key, buf8 ), digest_from_hex( "60756966479dedc6dd4bcff8ea7d1d4ce4d4af2e7b097e32e3763518441147cc12b3c0ee6d2ecabf1198cec92e86a3616fba4f4e872f5825330adbb4c1dee444" ) );
        TEST_EQ( test_hex<blake2b_512>( key, buf9 ), digest_from_hex( "e6fec19d89ce8717b1a087024670fe026f6c7cbda11caef959bb2d351bf856f8055d1c0ebdaaa9d1b17886fc2c562b5e99642fc064710c0d3488a02b5ed7f6fd" ) );
        TEST_EQ( test_hex<blake2b_512>( key, buf10 ), digest_from_hex( "142709d62e28fcccd0af97fad0f8465b971e82201dc51070faa0372aa43e92484be1c1e73ba10906d5d1853db6a4106e0a7bf9800d373d6dee2d46d62ef2a461" ) );
    }

    {
        constexpr char const buf4[] = "000102030405060708090a0b";

        BOOST_CXX14_CONSTEXPR auto const key = digest_from_hex( "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3fabcdefghijkl" );
        TEST_EQ( test_hex<blake2b_512>( key, buf4 ), digest_from_hex( "530a70d1166cd144b85c033089e0b3627ee6e1665ee8535270511dad931f023f26f10f446e25cdf5b4ac6906d7544fa88bd37a855cd4116282147355f67930ad" ) );
    }

    {
        constexpr char const buf1[] = "";
        constexpr char const buf2[] = "00";
        constexpr char const buf3[] = "000102";
        constexpr char const buf4[] = "000102030405060708090a0b";
        constexpr char const buf5[] = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f";
        constexpr char const buf6[] = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e";
        constexpr char const buf7[] = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f";
        constexpr char const buf8[] = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f80";
        constexpr char const buf9[] = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f808182";
        constexpr char const buf10[] = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f808182838485868788898a";
        constexpr char const buf11[] = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9fa0a1a2a3a4a5a6a7a8a9aaabacadaeafb0b1b2b3b4b5b6b7b8b9babbbcbdbebfc0c1c2c3c4c5c6c7c8c9cacbcccdcecfd0d1d2d3d4d5d6d7d8d9dadbdcdddedfe0e1e2e3e4e5e6e7e8e9eaebecedeeeff0f1f2f3f4f5f6f7f8f9fafbfcfdfe";

        TEST_EQ( test_hex<blake2s_256>( 0, buf1 ), digest_from_hex( "69217a3079908094e11121d042354a7c1f55b6482ca1a51e1b250dfd1ed0eef9" ) );
        TEST_EQ( test_hex<blake2s_256>( 0, buf2 ), digest_from_hex( "e34d74dbaf4ff4c6abd871cc220451d2ea2648846c7757fbaac82fe51ad64bea" ) );
        TEST_EQ( test_hex<blake2s_256>( 0, buf3 ), digest_from_hex( "e8f91c6ef232a041452ab0e149070cdd7dd1769e75b3a5921be37876c45c9900" ) );
        TEST_EQ( test_hex<blake2s_256>( 0, buf4 ), digest_from_hex( "1ac7ba754d6e2f94e0e86c46bfb262abbb74f450ef456d6b4d97aa80ce6da767" ) );
        TEST_EQ( test_hex<blake2s_256>( 0, buf5 ), digest_from_hex( "56f34e8b96557e90c1f24b52d0c89d51086acf1b00f634cf1dde9233b8eaaa3e" ) );
        TEST_EQ( test_hex<blake2s_256>( 0, buf6 ), digest_from_hex( "f18417b39d617ab1c18fdf91ebd0fc6d5516bb34cf39364037bce81fa04cecb1" ) );
        TEST_EQ( test_hex<blake2s_256>( 0, buf7 ), digest_from_hex( "1fa877de67259d19863a2a34bcc6962a2b25fcbf5cbecd7ede8f1fa36688a796" ) );
        TEST_EQ( test_hex<blake2s_256>( 0, buf8 ), digest_from_hex( "5bd169e67c82c2c2e98ef7008bdf261f2ddf30b1c00f9e7f275bb3e8a28dc9a2" ) );
        TEST_EQ( test_hex<blake2s_256>( 0, buf9 ), digest_from_hex( "e76d3fbda5ba374e6bf8e50fadc3bbb9ba5c206ebdec89a3a54cf3dd84a07016" ) );
        TEST_EQ( test_hex<blake2s_256>( 0, buf10 ), digest_from_hex( "ca9ffac4c43f0b48461dc5c263bea3f6f00611ceacabf6f895ba2b0101dbb68d" ) );
        TEST_EQ( test_hex<blake2s_256>( 0, buf11 ), digest_from_hex( "f03f5789d3336b80d002d59fdf918bdb775b00956ed5528e86aa994acb38fe2d" ) );

        // verified against openssl
        TEST_EQ( test_hex<blake2s_256>( 7, buf1 ), digest_from_hex( "ac2f7a8afebcaa24f988a3f1d4ed178edb7f94ee72b1778fc472926b0c4268bf" ) );
        TEST_EQ( test_hex<blake2s_256>( 7, buf2 ), digest_from_hex( "1d58d8954b7e2e5966be7c1fb6789bf50ff6ce9398c2b55e2d2b90e35a6e9141" ) );
        TEST_EQ( test_hex<blake2s_256>( 7, buf3 ), digest_from_hex( "30f76bbfcdd6a4909046853b6cd194bc50ccb7d8e1531300f4166ccd925d1847" ) );
        TEST_EQ( test_hex<blake2s_256>( 7, buf4 ), digest_from_hex( "9c713f17bb19098453b9dabb157c6e2c0dba1e351c96b6e0669597f905b890bf" ) );
        TEST_EQ( test_hex<blake2s_256>( 7, buf5 ), digest_from_hex( "b4d9edc4f069aa3dc57a51e3b2157ecb54744e3b9a2e5619cb7e7826aa0ae462" ) );
        TEST_EQ( test_hex<blake2s_256>( 7, buf6 ), digest_from_hex( "3f2c6c4e230891a0703140e9868815feec8b12dbf6ecde2539b03389099eb3bc" ) );
        TEST_EQ( test_hex<blake2s_256>( 7, buf7 ), digest_from_hex( "3a346bfdc453dd1d491b8e99219dc67b442c6f2afe5e212f2519084bc1e679af" ) );
        TEST_EQ( test_hex<blake2s_256>( 7, buf8 ), digest_from_hex( "28d35457654262614efb368ee4fba2695ac9e87d050f282f12bacc2a19253daa" ) );
        TEST_EQ( test_hex<blake2s_256>( 7, buf9 ), digest_from_hex( "a7a6d2e08c37c18f435b0c0fe96e27eb51c1f9d11e1f3faa841fd72910ac9c3f" ) );
        TEST_EQ( test_hex<blake2s_256>( 7, buf10 ), digest_from_hex( "ec43e1abf281d08abcb08ca2647f10f7dfa5093aa00b86c36741fee0e4de5096" ) );
        TEST_EQ( test_hex<blake2s_256>( 7, buf11 ), digest_from_hex( "01ec35f4e5b5d4ad82bc0c3ac0c8aae703078ea0a804ff102f1e2c2f9fe4f16f" ) );

        BOOST_CXX14_CONSTEXPR auto const key = digest_from_hex( "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f" );

        TEST_EQ( test_hex<blake2s_256>( key, buf1 ), digest_from_hex( "48a8997da407876b3d79c0d92325ad3b89cbb754d86ab71aee047ad345fd2c49" ) );
        TEST_EQ( test_hex<blake2s_256>( key, buf2 ), digest_from_hex( "40d15fee7c328830166ac3f918650f807e7e01e177258cdc0a39b11f598066f1" ) );
        TEST_EQ( test_hex<blake2s_256>( key, buf3 ), digest_from_hex( "1d220dbe2ee134661fdf6d9e74b41704710556f2f6e5a091b227697445dbea6b" ) );
        TEST_EQ( test_hex<blake2s_256>( key, buf4 ), digest_from_hex( "fba16169b2c3ee105be6e1e650e5cbf40746b6753d036ab55179014ad7ef6651" ) );
        TEST_EQ( test_hex<blake2s_256>( key, buf5 ), digest_from_hex( "8975b0577fd35566d750b362b0897a26c399136df07bababbde6203ff2954ed4" ) );
        TEST_EQ( test_hex<blake2s_256>( key, buf6 ), digest_from_hex( "ddbfea75cc467882eb3483ce5e2e756a4f4701b76b445519e89f22d60fa86e06" ) );
        TEST_EQ( test_hex<blake2s_256>( key, buf7 ), digest_from_hex( "0c311f38c35a4fb90d651c289d486856cd1413df9b0677f53ece2cd9e477c60a" ) );
        TEST_EQ( test_hex<blake2s_256>( key, buf8 ), digest_from_hex( "46a73a8dd3e70f59d3942c01df599def783c9da82fd83222cd662b53dce7dbdf" ) );
        TEST_EQ( test_hex<blake2s_256>( key, buf9 ), digest_from_hex( "ab70c5dfbd1ea817fed0cd067293abf319e5d7901c2141d5d99b23f03a38e748" ) );
        TEST_EQ( test_hex<blake2s_256>( key, buf10 ), digest_from_hex( "c7a197b3a05b566bcc9facd20e441d6f6c2860ac9651cd51d6b9d2cdeeea0390" ) );
        TEST_EQ( test_hex<blake2s_256>( key, buf11 ), digest_from_hex( "3fb735061abc519dfe979e54c1ee5bfad0a9d858b3315bad34bde999efd724dd" ) );
    }

    {
        constexpr char const buf4[] = "000102030405060708090a0b";
        BOOST_CXX14_CONSTEXPR auto const key = digest_from_hex( "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1fabcdefghijkl" );

        TEST_EQ( test_hex<blake2s_256>( key, buf4 ), digest_from_hex( "b65eb5252f5763675336bb29c380f25cf295fb66f7046d496768582ed53ebd2f" ) );
    }

    return boost::report_errors();
}
