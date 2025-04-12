// Copyright 2024 Christian Mazakas.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#define _CRT_SECURE_NO_WARNINGS

#include <boost/hash2/sha2.hpp>
#include <boost/core/lightweight_test.hpp>
#include <cstdio>
#include <string>
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

template<class H> std::string digest( std::string const & s )
{
    H h;

    h.update( s.data(), s.size() );

    return to_string( h.result() );
}

template<class H, std::size_t N> std::string digest( char (&buf)[N])
{
    H h;

    h.update( buf, N );

    return to_string( h.result() );
}

template<class H> std::string digest( std::vector<char> const & v )
{
    H h;

    h.update( v.data(), v.size() );

    return to_string( h.result() );
}

static void sha_256()
{
    using boost::hash2::sha2_256;

    // https://en.wikipedia.org/wiki/SHA-2#Test_vectors

    BOOST_TEST_EQ( digest<sha2_256>( "" ), std::string( "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" ) );

    // https://csrc.nist.gov/CSRC/media/Projects/Cryptographic-Standards-and-Guidelines/documents/examples/SHA256.pdf

    BOOST_TEST_EQ( digest<sha2_256>( "abc" ), std::string( "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" ) );
    BOOST_TEST_EQ( digest<sha2_256>( "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq" ), std::string( "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1" ) );

    // use test vectors from here
    // https://csrc.nist.gov/CSRC/media/Projects/Cryptographic-Standards-and-Guidelines/documents/examples/SHA2_Additional.pdf

    BOOST_TEST_EQ( digest<sha2_256>( "\xbd" ), std::string( "68325720aabd7c82f30f554b313d0570c95accbb7dc4b5aae11204c08ffe732b" ) );
    BOOST_TEST_EQ( digest<sha2_256>( "\xc9\x8c\x8e\x55" ), std::string( "7abc22c0ae5af26ce93dbb94433a0e0b2e119d014f8e7f65bd56c61ccccd9504" ) );

    {
        char buf[ 55 ] = { 0 };
        BOOST_TEST_EQ( digest<sha2_256>( buf ), std::string( "02779466cdec163811d078815c633f21901413081449002f24aa3e80f0b88ef7" ) );
    }

    {
        char buf[ 56 ] = { 0 };
        BOOST_TEST_EQ( digest<sha2_256>( buf ), std::string( "d4817aa5497628e7c77e6b606107042bbba3130888c5f47a375e6179be789fbb" ) );
    }

    {
        char buf[ 64 ] = { 0 };
        BOOST_TEST_EQ( digest<sha2_256>( buf ), std::string( "f5a5fd42d16a20302798ef6ed309979b43003d2320d9f0e8ea9831a92759fb4b" ) );
    }

    {
        char buf[ 1000 ] = { 0 };
        BOOST_TEST_EQ( digest<sha2_256>( buf ), std::string( "541b3e9daa09b20bf85fa273e5cbd3e80185aa4ec298e765db87742b70138a53" ) );
    }

    {
        char buf[ 1000 ] = {};
        for( auto& c : buf ) c = 'A';
        BOOST_TEST_EQ( digest<sha2_256>( buf ), std::string( "c2e686823489ced2017f6059b8b239318b6364f6dcd835d0a519105a1eadd6e4" ) );
    }

    {
        char buf[ 1005 ] = {};
        for( auto& c : buf ) c = 'U';
        BOOST_TEST_EQ( digest<sha2_256>( buf ), std::string( "f4d62ddec0f3dd90ea1380fa16a5ff8dc4c54b21740650f24afc4120903552b0" ) );
    }

    {
        std::vector<char> buf(1000000, 0x00);
        BOOST_TEST_EQ( digest<sha2_256>( buf ), std::string( "d29751f2649b32ff572b5e0a9f541ea660a50f94ff0beedfb0b692b924cc8025" ) );
    }
}

static void sha_224()
{
    using boost::hash2::sha2_224;

    // https://en.wikipedia.org/wiki/SHA-2#Test_vectors

    BOOST_TEST_EQ( digest<sha2_224>( "" ), std::string( "d14a028c2a3a2bc9476102bb288234c415a2b01f828ea62ac5b3e42f" ) );

    // https://csrc.nist.gov/CSRC/media/Projects/Cryptographic-Standards-and-Guidelines/documents/examples/SHA224.pdf
    BOOST_TEST_EQ( digest<sha2_224>( "abc" ), std::string( "23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7" ) );
    BOOST_TEST_EQ( digest<sha2_224>( "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq" ), std::string( "75388b16512776cc5dba5da1fd890150b0c6455cb4f58b1952522525" ) );

    // https://csrc.nist.gov/CSRC/media/Projects/Cryptographic-Standards-and-Guidelines/documents/examples/SHA2_Additional.pdf
    BOOST_TEST_EQ( digest<sha2_224>( "\xff" ), std::string( "e33f9d75e6ae1369dbabf81b96b4591ae46bba30b591a6b6c62542b5" ) );
    BOOST_TEST_EQ( digest<sha2_224>( "\xe5\xe0\x99\x24" ), std::string( "fd19e74690d291467ce59f077df311638f1c3a46e510d0e49a67062d" ) );

    {
        char buf[ 56 ] = { 0 };
        BOOST_TEST_EQ( digest<sha2_224>( buf ), std::string( "5c3e25b69d0ea26f260cfae87e23759e1eca9d1ecc9fbf3c62266804" ) );
    }

    {
        char buf[ 1000 ] = {};
        for( auto& c : buf ) c = 'Q';
        BOOST_TEST_EQ( digest<sha2_224>( buf ), std::string( "3706197f66890a41779dc8791670522e136fafa24874685715bd0a8a" ) );
    }

    {
        char buf[ 1000 ] = {};
        for( auto& c : buf ) c = 'A';
        BOOST_TEST_EQ( digest<sha2_224>( buf ), std::string( "a8d0c66b5c6fdfd836eb3c6d04d32dfe66c3b1f168b488bf4c9c66ce" ) );
    }

    {
        char buf[ 1005 ] = {};
        for( auto& c : buf ) c = '\x99';
        BOOST_TEST_EQ( digest<sha2_224>( buf ), std::string( "cb00ecd03788bf6c0908401e0eb053ac61f35e7e20a2cfd7bd96d640" ) );
    }

    {
        std::vector<char> buf(1000000, 0x00);
        BOOST_TEST_EQ( digest<sha2_224>( buf ), std::string( "3a5d74b68f14f3a4b2be9289b8d370672d0b3d2f53bc303c59032df3" ) );
    }
}

static void sha_512()
{
    using boost::hash2::sha2_512;

    // https://en.wikipedia.org/wiki/SHA-2#Test_vectors
    BOOST_TEST_EQ( digest<sha2_512>( "" ), std::string( "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e" ) );

    // https://csrc.nist.gov/CSRC/media/Projects/Cryptographic-Standards-and-Guidelines/documents/examples/SHA512.pdf
    BOOST_TEST_EQ( digest<sha2_512>( "abc" ), std::string( "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f" ) );
    BOOST_TEST_EQ( digest<sha2_512>( "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu" ), std::string( "8e959b75dae313da8cf4f72814fc143f8f7779c6eb9f7fa17299aeadb6889018501d289e4900f7e4331b99dec4b5433ac7d329eeb6dd26545e96e55b874be909" ) );

    // https://csrc.nist.gov/CSRC/media/Projects/Cryptographic-Standards-and-Guidelines/documents/examples/SHA2_Additional.pdf
    {
        char buf[ 111 ] = {};
        for( auto& c : buf ) c = 0;
        BOOST_TEST_EQ( digest<sha2_512>( buf ), std::string( "77ddd3a542e530fd047b8977c657ba6ce72f1492e360b2b2212cd264e75ec03882e4ff0525517ab4207d14c70c2259ba88d4d335ee0e7e20543d22102ab1788c" ) );
    }

    {
        char buf[ 112 ] = {};
        for( auto& c : buf ) c = 0;
        BOOST_TEST_EQ( digest<sha2_512>( buf ), std::string( "2be2e788c8a8adeaa9c89a7f78904cacea6e39297d75e0573a73c756234534d6627ab4156b48a6657b29ab8beb73334040ad39ead81446bb09c70704ec707952" ) );
    }

    {
        char buf[ 113 ] = {};
        for( auto& c : buf ) c = 0;
        BOOST_TEST_EQ( digest<sha2_512>( buf ), std::string( "0e67910bcf0f9ccde5464c63b9c850a12a759227d16b040d98986d54253f9f34322318e56b8feb86c5fb2270ed87f31252f7f68493ee759743909bd75e4bb544" ) );
    }

    {
        char buf[ 122 ] = {};
        for( auto& c : buf ) c = 0;
        BOOST_TEST_EQ( digest<sha2_512>( buf ), std::string( "4f3f095d015be4a7a7cc0b8c04da4aa09e74351e3a97651f744c23716ebd9b3e822e5077a01baa5cc0ed45b9249e88ab343d4333539df21ed229da6f4a514e0f" ) );
    }

    {
        char buf[ 1000 ] = {};
        for( auto& c : buf ) c = 0;
        BOOST_TEST_EQ( digest<sha2_512>( buf ), std::string( "ca3dff61bb23477aa6087b27508264a6f9126ee3a004f53cb8db942ed345f2f2d229b4b59c859220a1cf1913f34248e3803bab650e849a3d9a709edc09ae4a76" ) );
    }

    {
        char buf[ 1000 ] = {};
        for( auto& c : buf ) c = 'A';
        BOOST_TEST_EQ( digest<sha2_512>( buf ), std::string( "329c52ac62d1fe731151f2b895a00475445ef74f50b979c6f7bb7cae349328c1d4cb4f7261a0ab43f936a24b000651d4a824fcdd577f211aef8f806b16afe8af" ) );
    }

    {
        char buf[ 1005 ] = {};
        for( auto& c : buf ) c = 'U';
        BOOST_TEST_EQ( digest<sha2_512>( buf ), std::string( "59f5e54fe299c6a8764c6b199e44924a37f59e2b56c3ebad939b7289210dc8e4c21b9720165b0f4d4374c90f1bf4fb4a5ace17a1161798015052893a48c3d161" ) );
    }

    {
        std::vector<char> buf(1000000, 0x00);
        BOOST_TEST_EQ( digest<sha2_512>( buf ), std::string( "ce044bc9fd43269d5bbc946cbebc3bb711341115cc4abdf2edbc3ff2c57ad4b15deb699bda257fea5aef9c6e55fcf4cf9dc25a8c3ce25f2efe90908379bff7ed" ) );
    }
}

static void sha_384()
{
    using boost::hash2::sha2_384;

    // https://en.wikipedia.org/wiki/SHA-2#Test_vectors
    BOOST_TEST_EQ( digest<sha2_384>( "" ), std::string( "38b060a751ac96384cd9327eb1b1e36a21fdb71114be07434c0cc7bf63f6e1da274edebfe76f65fbd51ad2f14898b95b" ));

    // https://csrc.nist.gov/CSRC/media/Projects/Cryptographic-Standards-and-Guidelines/documents/examples/SHA384.pdf
    BOOST_TEST_EQ( digest<sha2_384>( "abc" ), std::string( "cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7" ) );
    BOOST_TEST_EQ( digest<sha2_384>( "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu" ), std::string( "09330c33f71147e83d192fc782cd1b4753111b173b3b05d22fa08086e3b0f712fcc7c71a557e2db966c3e9fa91746039" ) );

    // https://csrc.nist.gov/CSRC/media/Projects/Cryptographic-Standards-and-Guidelines/documents/examples/SHA2_Additional.pdf
    {
        char buf[ 111 ] = {};
        BOOST_TEST_EQ( digest<sha2_384>( buf ), std::string( "435770712c611be7293a66dd0dc8d1450dc7ff7337bfe115bf058ef2eb9bed09cee85c26963a5bcc0905dc2df7cc6a76" ) );
    }

    {
        char buf[ 112 ] = {};
        BOOST_TEST_EQ( digest<sha2_384>( buf ), std::string( "3e0cbf3aee0e3aa70415beae1bd12dd7db821efa446440f12132edffce76f635e53526a111491e75ee8e27b9700eec20" ) );
    }

    {
        char buf[ 113 ] = {};
        BOOST_TEST_EQ( digest<sha2_384>( buf ), std::string( "6be9af2cf3cd5dd12c8d9399ec2b34e66034fbd699d4e0221d39074172a380656089caafe8f39963f94cc7c0a07e3d21" ) );
    }

    {
        char buf[ 122 ] = {};
        BOOST_TEST_EQ( digest<sha2_384>( buf ), std::string( "12a72ae4972776b0db7d73d160a15ef0d19645ec96c7f816411ab780c794aa496a22909d941fe671ed3f3caee900bdd5" ) );
    }

    {
        char buf[ 1000 ] = {};
        BOOST_TEST_EQ( digest<sha2_384>( buf ), std::string( "aae017d4ae5b6346dd60a19d52130fb55194b6327dd40b89c11efc8222292de81e1a23c9b59f9f58b7f6ad463fa108ca" ) );
    }

    {
        char buf[ 1000 ];
        for( auto& c : buf ) c = 'A';
        BOOST_TEST_EQ( digest<sha2_384>( buf ), std::string( "7df01148677b7f18617eee3a23104f0eed6bb8c90a6046f715c9445ff43c30d69e9e7082de39c3452fd1d3afd9ba0689" ) );
    }

    {
        char buf[ 1005 ];
        for( auto& c : buf ) c = 'U';
        BOOST_TEST_EQ( digest<sha2_384>( buf ), std::string( "1bb8e256da4a0d1e87453528254f223b4cb7e49c4420dbfa766bba4adba44eeca392ff6a9f565bc347158cc970ce44ec" ) );
    }

    {
        std::vector<char> buf(1000000, 0);
        BOOST_TEST_EQ( digest<sha2_384>( buf ), std::string( "8a1979f9049b3fff15ea3a43a4cf84c634fd14acad1c333fecb72c588b68868b66a994386dc0cd1687b9ee2e34983b81" ) );
    }
}

static void sha_512_224()
{
    using boost::hash2::sha2_512_224;

    // https://en.wikipedia.org/wiki/SHA-2#Test_vectors
    BOOST_TEST_EQ( digest<sha2_512_224>( "" ), std::string( "6ed0dd02806fa89e25de060c19d3ac86cabb87d6a0ddd05c333b84f4" ));

    // https://csrc.nist.gov/CSRC/media/Projects/Cryptographic-Standards-and-Guidelines/documents/examples/SHA512_224.pdf
    BOOST_TEST_EQ( digest<sha2_512_224>( "abc" ), std::string( "4634270f707b6a54daae7530460842e20e37ed265ceee9a43e8924aa" ) );
    BOOST_TEST_EQ( digest<sha2_512_224>( "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu" ), std::string( "23fec5bb94d60b23308192640b0c453335d664734fe40e7268674af9" ) );

    // selected samples from the download available here:
    // https://csrc.nist.gov/Projects/Cryptographic-Algorithm-Validation-Program/Secure-Hashing
    std::pair<char const*, char const*> inputs[] =
    {
          /* expected hash */                                         /* input message */
        { "4199239e87d47b6feda016802bf367fb6e8b5655eff6225cb2668f4a", "cf" },
        { "392b99b593b85e147f031986c2a9edfdb4ffd9f24c77c452d339c9fc", "ca2d" },
        { "a9c345d58a959af20a42c84e28523ba47e3bf8fad8e8c3f32b7a72ae", "497604" },
        { "a74af68819afe81bcdaceba64201c0d41f843e4b08e4002a375be761", "3c7e038401fa74c6c06e41" },
        { "087ed68f1db90ffb2fb4ff7dc4b17fe08100b64383850378ef543339", "40bd7d47b636c2a749a247fdda75807c238b" },
        { "72b43417b071f4811833027731b0ca28549c0357530fe258ca00533e", "cd5fee5fde5e9aa2884b4f4882cfa7d5571f8fd572c5f9bf77a3d21fda35" },
        { "fa425bc732d6033566c073560b2c5fe322aa4fa22aaa3ec51154ffd8", "f15284a11c61e129ea0606bd6531f2f1213776e01e253d1def530bed1c3c42b3c68caa" },
        { "75a0fa978c45d268124d8cd9ef0a08ecabbbed53412cfc7cb1c00398", "e3089c05ce1549c47a97785d82474c73cb096a27c5205de8ed9e3a8c971f7fa0eab741fd2c29879c40" },
        { "1e7590e408c038b794e9820b25d011c262062b96d111dccc46dc6783", "0431a7bfbbec1bb8116a62e1db7e1346862d31ad5110ff1bb9fa169a35dbb43a24e4575604ec8b18e41300" },
        { "33a5a8d6119bb6dd7b2e72ece8e4d5d02aa99048c0459169ee9e6d04", "4dbe1290524bd73d9db5f21f9d035e183dc285b85ba755057c769777be227c470e3679ea9a7355d889bb8191ea2ea7e2" },
        { "09900c5ae3074fe73e6c4eef51f785e57947bafbe1d8dea38868e3d1", "13e6b1b4f021d610c81c97f0f952daba2766034d815b5dda4603bcf788ba60ee31541d5b4353b9f6645d96ad99ee90f6524b2963a7b7e476e1e8eeb83cbc0305eb29902a5d72" },
        { "9a9176e97aec99ab07f468f6a226876710d6d877021d27061d4d0132", "4b9895235cb4956aefffe815415252e7d6b21921bd7f675315eff071d0bbd429b718c774aee96f6c3a330d5d40d1601e1069c7a2a19ea5ca1e87097da2608ffb4180816e478b42c3c4e9edb748773935eb7ca0df90dec0eb6b960130c1617880efb80b39ae03d617950ace4ce0aca4d36fd3ed0112a77f5d03021eb1b42458" },
        { "72640a79fbb1cfb26e09b4b35385389ed633a55e092906d01a7186e1", "9625ae618ea633fd7ae5b20ceafd6b1f3ab1a6aa20aded66810e78f38925e9c2fa783a32c40af3f9d7dda0c635b482254b1d85a281af7231109166cd133c8360e281e5e39bcdd7c601ac47928a8c78cdb3c4f71e97d4d0b1c0ee01dd3db62f04f44798bb3a76492ba15a91b7110cb5e01babe56589a36fae3a2f336a2d1d5778dbd23c03ca8db0f25ff0657ff4bca1252adc38c080a5b8f0255ce3be0bf862823d2ab704729b74e1e275aa305824a566895ed677a460113e2a7bf91f00d0b8ebc358f3035b27fcc1d3f14a1367cd2769df39a9d21c5ee361f1965cd6342cc17a1463d6" },
        { "13f224102f57b2a5774d979be2ff6691a9ae7125a1443e805598abe9", "1e13f580fa2de14f1294e1eba9ce789c75072c9f54fcdc253c17549d70db1f36fa839ca055e655136a9e8b93be691672e9cf6c164a06fffdf912ccfdfd3030d1bf75fed1bfb3d001869c9a4ddc2a85133a3efae28287fc82eea5bc34468e673a5731439aa05afa204ed636a26bd76d87529aff8a66467ebc03184cc8b5bd6c7ba8ff928460a47c78aa938519d33978d7172ba2975c0d2bb421b2a643b184e69c9c2713166759fe11831db23a7c184c0a733b0c90cea2ab712ebcef2da1ad7ea31af0f0d81e4127f4bfbae38dce3c91284d1064fd23cea7fb137e520ceffedb9a09a44e52eb23a02848b3419b326cf03a8cf3d367c359c75bb940f56a0240a6885389580d37c910b50450f3eb0e7210471a2c8155160bb298074a00ce423a8676dfe733906bf920a4ddd82105cb149b57de03954f84ac11bae4e39cc117b6246b95080a63ed7c78fcba95f572d21b3673c0c037dd75038bca3a55f1cae97a276f5d33030f271abfc582cd95b98a4e8ba1a8aa918851d9c9cef0e626712050388a1faf461b7f9a9e071fc929625a7742eb7e0ac8d780f672d1eedf633e24feffc5c3c5fc0f5fc6bccb78d1daf6ac5c03592a2807536a222fd81c88d2ba5e4c232731bd4d742e64c218752ccffce7c775f2954c74827725a8ed6228986c34044db952df60d0543d57f7fe2432fc727e40bea5be37ee10c68417a808e0dc0fec24820c725eae3919246ffbff287eb7490dc543c5791f9bce6c5fd671d09358ce518a48c06e9345c0d0885406db0b1df2841058ab629c820607cbd4a9901875707bd9ef0fb909e0f9044af281732a0c3f4ca6cab619ed2b33fe668af849f3d09a4b78e7a86797728d68bebbb81764ecfa0e8f832fc79a0020f835e0a823adcf16686e1ad39fc66345a3eb98f2f04026e971d4695e932b67949e42bf0045cddcab7cc36ed8891c1be75eda7179264620d51e7c668aa629ca5698940d47d8db5e202c7fcdfe4e23023b40912f93e0fe4385bc8f61fc271c902f8d33b4c60360bd3e22ab1e8e1663800bf21ab88946f1f7c3f41641fd3f8a21e725289ac5efe360cd064f49d00863fe1d1b7df6a382a6a5556e17f0d316fde546097aa98bc3ecb1957f71350c4dabe23a64c0d028b9a5b304d19d55c6d3fc44" },
        { "8f7b464921f5344b3ea2f07ea95028bf0ef23573d1025160caa5b000", "cd4c8dbfb9cb8b99f4fe7c65f027ed8f3f1da8c79a41d89c27b46f4cfcc7c5dadfb381624f8bbdaac96e65f94401e02cfe1cf3067ac4e2c8c95ad8010a1188a82da61568fc4b2f3b5be4803ace7546b43ddb417df4bbcf88eeb956b853eae2f43f8b0f7ac5f32a7534c693d1986c654e84c159d30f0ee8061d39b019e03819e69ecb383fb46bf47fdb7d6765aacc87235c3605b1d4a67567de42b0cbb8e576a8d1cf5860d09bd582d872c49f0a7ff25a4ca20bbf4969bed6b93c1c77e3d7415f60fe3784216b17a6b40c7127c26bee1cdd6e34a478400a79378fbdc46af8c236d4bf9c54b0c40112f6b238a4da7ac591950048096913a378dfa35b5b542e4153b37c5177848cdaef26cedfbf5893bde0ffc10f9523cbcffc3ae0bee3a96305de889c1c96a5adeb64ccc72f1f469d2032a01951f1ae4ed72dceafad05717e5700d887c6591c0d7c9a7105f06cab8234ae0271d5c4128603322676f169b5677fe02da41767f096d4b99cf0ea509029427dac2f64b7575a9a34c79e52b67d06c5a80e5dd669292e3de8d6f0e471fc7a91c626bdc67286b1e65a3b909895cf43707d0419cd06d0bffd4fa66da877f75d6bb2f981c39ca7752dcd09b38badb506ca899641c2e7ec305d1794cffc0b8aba1669ace522ddc08db8b4b96ef3a7363719f44c3b7eb48569a568287ea9d5bb79db12c74faddfdbe1cf38d32b972a5abea6e6ae70c914a4521d69ebe2777c948202d5d2292a47ff59b981e05140df2a83b98645486aaf4fd30d4364632da91dbdd3db56da62c035ddc0b66848b66dad6e9afc982c2b2e91d4bc1d1c6a95035d13964cdbca1df084552afff177c17eec6890712f82d2396f3d7320995d75335c6a9250e761237ff4a94085054829b7ed57e40d93b32877f3f67a7465a696e022ac100d04073b168dc14a702b22b5c2ba25dff74b28f59124e7194008abc0b3b8bf58aeb9242f5d588590e8ec6d2f475bb8c658df48012e0af998ac08d6ba53258db2598e20c5fb5cde5914ce925dedd6fae457d87a13f7ae123ee2ef8e4e6a71fc66370c63c699a1b2ef1c5bf7075d35d1801dbd28f594171b5407443a429da5f6bb6fba55f9de30eb98291b05f73f8027e1d116b96dac4dfd224690445f96c762fa29215f2873106f9d639524f43abde3509c6c9544e7599ef0c55a136dfde2276c8fc7e719ff492846f151bdc5f6f6ed15a6452442ef42e806ac2a0f3479fb2f56c63657952be4fcdafbd736331c322d78162ccd2e6910c2ab2488a07bb31c6103f9f615649fe8d5a3abb0f906547a8bc114a6fbd100cc132955fd2e0c534ca5ba4e8d8b4e025d9ae727636c0645d5ef37ca3d13f45a7dc5c6661021ad0094e3c2ed851f1bfc4c33ef9778f9fa984b41235e787e5d1f2bbfcb7f4ae12762fa0364ff663b9237bc8703247707acf4e469cdf8dfa4d5f8dee980ca32d6422289eee8acb6467d58b5806af6e9fde202a94f7dc1fbed238d9f2b2c6c5069c5e8468a961bffa8a20a3f056e345645656aa1db7be053c21185756d18902c007a5b3bc0b575c8c8c9f363ff55064446fdd3d4f60e7e7089658869978d5c4f0527246f798fe19a10bcbb285468953d6871e54b8680c3d8ace8408291d1f840de95ebea9b6a88fce7420b97edef09c8138b490d7417d615916252c8432c8d87f58c57b50f8f9276b5228bb6b2328a6eaf11a722a91ce0afa29b694396e843adb0d410d7be462690e325e28f8b783eb5a4cca6930cd6082c455a2bfc704c41397934bf588fd76cd3d713b82a610b50285b84ef67848f7716e46940fabb9fb0b3b7750c4169898dc6fab4b78d1a31e64c3a012aa6fb8d54f4e68e51" },
    };

    for( auto const& input : inputs )
    {
        BOOST_TEST_EQ( digest<sha2_512_224>( from_hex( input.second ) ), std::string( input.first ));
    }
}

static void sha_512_256()
{
    using boost::hash2::sha2_512_256;

    // https://en.wikipedia.org/wiki/SHA-2#Test_vectors
    BOOST_TEST_EQ( digest<sha2_512_256>( "" ), std::string( "c672b8d1ef56ed28ab87c3622c5114069bdd3ad7b8f9737498d0c01ecef0967a" ) );

    // https://csrc.nist.gov/CSRC/media/Projects/Cryptographic-Standards-and-Guidelines/documents/examples/SHA512_256.pdf
    BOOST_TEST_EQ( digest<sha2_512_256>( "abc" ), std::string( "53048e2681941ef99b2e29b76b4c7dabe4c2d0c634fc6d46e0e2f13107e7af23" ) );
    BOOST_TEST_EQ( digest<sha2_512_256>( "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu" ), std::string( "3928e184fb8690f840da3988121d31be65cb9d3ef83ee6146feac861e19b563a" ) );

    // selected samples from the download available here:
    // https://csrc.nist.gov/Projects/Cryptographic-Algorithm-Validation-Program/Secure-Hashing
    std::pair<char const*, char const*> inputs[] =
    {
          /* expected hash */                                                 /* input message */
        { "c4ef36923c64e51e875720e550298a5ab8a3f2f875b1e1a4c9b95babf7344fef", "fa" },
        { "0c994228b8d3bd5ea5b5259157a9bba7a193118ad22817e6fbed2df1a32a4148", "74e4" },
        { "a9e2427cec314b2814aaba87039485fc8d3ade992fa1d9acbb7f6769460a7317", "6f63b4" },
        { "447044f03bc30e2caa245d26ce4c72c1454f708cfcd9a215841a88cf5ecd2095", "7dae8fc020d9" },
        { "5424b31989031809b5fc8969b7c48b5dc0c233fc34ffb5b223cd5f3a9712a8d6", "8d2782a7843aa477b8f3bca9f9f2bafb5813db4c8c43" },
        { "41c7027d16e37259645d0173c86141f38d808e9e27dc2dfeeaf335ed7c99490c", "95de55287ad3cff69efec6e97c812456e47be25e433470c3259b" },
        { "683b486861e598dabba740ac919522cf3b609c18205b6beca4ccbe6b0f6dc6db", "1b77c8dcfd2fc4b54617054fa6b14d6e9d09ce9185a34a7fd2b27923998aab99" },
        { "035d362a4265031434c3aa153ab4aef6b00d5176227197430bb62884a085c5fe", "a52dde5aea1f04399c5d91fb5c4e62da06b73a5d9bdc5a927fe85298e58e166187f154b69ac1057c01a7" },
        { "cd1040a0d94382a6446a691c0a07c4f643edc11e3103e343608603b5bd101be3", "b7ae6037d9b452994fb755aa0cf57fbc687298060a471f9b0c3d8e9d5a9ae0ad5b3b4fa7d4e3628c09d327c0259bbd76db2f" },
        { "e1470b2f44e3955d2b5496292679d9086b7c0aa94f0d516e53a142e486c6ded8", "e26e1dc893fdced833eae15ac04ed4624a07e3c89dc3d1d8205be44bfd63327782ff1bc2c8f7175c920eb22e7790d40d442b46349cff72f1e1" },
        { "00ce3b592d4e1a65f780df351fa7b2c01b49df4ea913c3fab24297f5791b18e5", "97e003903bb971a523ce0c82bda5d6733c76b90deb307559c1bddd35368743f6563b315214cd5a7ee0bccf937c9776360bc0b9786b707bfbc4fb50576155edbbbfd5ddd8e43a76faf2ec0c78fc84644f188d6b0ab68c28e5303ff031a223d9fafb3871e85408af6381e629fae67488068c68398a758f665e2c12258d9ff8effb31ec534b0c40ebffb43390e1e26fcaa28fd68ac24f7e1cafe0fa573103dc17058a77edc9b3ea1418b45aa7f5977e126d4861c778ed6332217581eee674d739622e63a529f10c11f4a9e3d8feaea848ade0905675f6458ffa132f52749af23d584438e5" },
        { "f3e524f645881c45318030624baf254fc6b3389d7bb7a22aa55ef528c2fd3cf0", "773e1098b725ab1a7465c67892a384147bf0c32714eeab05f13487a3c5f4d4561cdb98dd4c39f6a2a862fe3df854ca3a269ac61c3a704fa18827804828cb4811a704c084fc3abcf4befca10594a3766cec323df6f08183ccb59b36f5b664b71c827f3094b16e28991cfb54d94ed7b0333582cfbe1d6a6f0de05751b15606480013148f15521f5f182c27c1c00e3aa2156950257bab743dec6f247a85f0fb2344c48d8610c7938cf9554890c2bc12719cf065e63581e412f1cbca59776d897170fcb1bc8bf812d5c5fe2569e740a848503389bbf4870519a55c119592b3f95a0d2247da91cde662039187f23882444db898834cc1a51e778f8ac1d6dbab9305a2a01887272e565c3b7536d6bc6a2ea85c881d40a3c3765738bea1c65c3d9e9bb7ddec6c5703a9ebf612fabb7be1ad82c46c1d40a3c956df265e7a0b1c526443d4418a30283428" },
        { "03e58e78284c485bb6b54b36c0e12695b30bceced7bcf3ed6c435caf09875202", "2c2d86197b39c194c65a93bdadc72a9e590fa75fa8ae6b758adf9116abb2bbf0525e121a89d8884b7ea5614a29ecf1a52fcfa7a606fb4a835a7e83d5b0da9c4b4a5787a04a9d22ca1a81b9750a20ccaf3b2a13e5bd81c00ef403042640a3d4dedcbfed9207ae167b6298a5269a8a442446c9f5041367547e1e151cf1a138ed10d23f4dd70833ecc5208f2c451f53e06d185454ea51d68f3ba15cd41e1cbf6f48f37eae5995552d0f1a1156bae2a229f079723981bef5f7ad45823395f961e0f0062518fff63b60c319c42c6d2ba0a61631cbabb0a4ed163dd12c423f8045ca6fd4db93b1720f0d48e550b5464274df470708b25f49e574510f040802f994b42d2549285752f6bc06465efa2e6e485cecc5d15676f6069753c5b28dc7ab752792c016dedadf1af6650496010bf841a66b31cda0def37809a9bcded977df11aa066febbe205ea342cde69fd4c72889442e14a5977d886252bdbc2ff5f8dd8fc5f1f870ce121ab929a6b6227b484648be9b3501443cfdecf8f58d4de834ed1800bb244c18985a8232583ac6fc789aa59d1c5e87ad03994085bbf6e1ba1157d4e4ccbb28a49b6529e54b3b" },
        { "1a120db1b48a4035395b5f52e52decb1cc9c0c508b411455a9e33446f4d8a87c", "4389f52995e81550cc7ee17fc546bba08e9ed338704248f32d7b9084869419ab008e3097e815236f95b2a22ced416ba92ba627fddfbc8e08358ed24c7bca3bfede720e5d74d5b44380be18faf1eb51502bc081ee0a3dfde5242eae8a4fa1b784649d5723421111f4150b0b92d82ab5be1856aab2fbaedbb382c2eae60b03d4641a78d39058b283a9dc01439baae8de1b79cf6bf2baf87199d5b44f07b6d7dbc788694a4dc09e508a5e0887be6f0c158ed7938a564d0107c0a76ba596580514d437d9830282bfc0c02289a1a3307ea23fb90acf85beb3e5507f4fb43afa74c7b4ae314756a4fb1d73b0ea57f1f526cd57a79c042815f55270e6c8439ca20e3c8e75d75ef1303cadca91957250c4ecdefe29b1efe658ae9848633e1c635bbabb8b1535907c2d69baee4cf0ba6aa7b81c5bc8b15a6891c8fd017e1905382a608bd315b1a59fa46a8a076965f6e4a572d0970b040b56c72acac3bfc1c7c319e9f58dfdb564019c27653c715bf069c60242d97b12ca5703bc46b5af9f2aab2f3b41e256a8964b1d8795b5eb3a6e9f1350ba4a9e4361134c66bb00d519f599743fb7f9a6f203243075619b5f69392e93448012ff009633aa106fcbb998f234f1dd9efb78fb7335dbd61016555219bf4fc7db5823af335f25a2ab3fe485e725adfe400f512aae262893eb4d565cd5fa4dbf154494bafe3ec604b784dd469d13b088e35ebbe97b9e1f943a69e9ef3786" },
        { "0b09d2283b77d04185ea0d0cfde64f56e8d5e659e8c7dafdafaf1bcfa5565941", "e4d16c654f7edbf7372ed6297b135462f96051ffa70a7e3ce0cb3758fb6db5f4224585bd61fe384afe985d4709afacd433018428e42b987a87b3e521294be7fa42540c0ad2a26964b07fab4d1ff2519fa341196b13b4b8e2ee9cce76f294193ceb07e995caf2d40df6ebaa83e763ac8bf028f8105c2d483ebd8edc9e81cfb583c66504a67077e20d78e54131868a235fc3be0a5fe81faf549c92f6e3fc8422977b764584ecb8fb2bdd74589e27ef1fe5c30c359fdf1255ec31ae048b8c3cc840b1429579673d8830cea1d813f01b09e39919621a698ca2d027ecefdf587d0f4610d2dacb098fefe5f273a7d809954722eced7a16ac17fec724549d2789d25f70422dfec3f9359a0ff2f2ae359be5436a659163dcd33ad198d0edf7ab46c02087ec5be7a960b7ef797526d572ce946382ac0c07b4047e830cd5bfe5b2b8d1ba54b9657fc5659e58ea241805d6c929113468664ce567697cebd8d25086068c959c4e52ac5e31713c29a7cf24aab52cb44641a81cb9268524fd842f1e6d5979192a562a58beebb803ffa5531b36b925b381e1d95e76665ae4097edd39b0d921afc577f56a2bbcfc1434c6add3c2fe9d78aded01fa65526e22d971e48797ad0bbd3459bccaeeccc3d990b3e43396ac1c6a921028d7ce0f4c4733034df269febfad40a587604f4298b9db140022faea28f1f7f057dff9c879bce0dc1f6170f3c6e5b1035c149546001cdc900a8a35e04b75a63596f92da37ee8a8a562bbf2b5a7cf4f1a45ce0a02856a5cd0095290b70c5e6588eb0db4c8e43252061eea386bead1e1934976f49bae0c237c05625397f315fd916004d4603304a8a65cd58a3e1855311570f49c3c0368f93b06a2c6a78900" },
    };

    for( auto const& input : inputs )
    {
        BOOST_TEST_EQ( digest<sha2_512_256>( from_hex( input.second ) ), std::string( input.first ));
    }
}

int main()
{
    sha_256();
    sha_224();
    sha_512();
    sha_384();
    sha_512_224();
    sha_512_256();
    return boost::report_errors();
}
