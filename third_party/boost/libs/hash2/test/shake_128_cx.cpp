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

    unsigned char buf[M] = {};
    for( unsigned i = 0; i < M; ++i ) {
        auto c1 = to_byte( str[ 2 * i ] );
        auto c2 = to_byte( str[ 2 * i + 1 ] );
        buf[ i ] = ( c1 << 4 ) | c2;
    }

    h.update( buf, M / 3 );
    h.update( buf + M / 3, M - M / 3 );

    return h.result();
}

template<std::size_t D, class H> BOOST_CXX14_CONSTEXPR boost::hash2::digest<D> get_digest_result( H& h )
{
    boost::hash2::digest<D> r;

    std::size_t n = 0;

    for( ;; )
    {
        auto r2 = h.result();

        if( r2.size() >= r.size() - n )
        {
            boost::hash2::detail::memcpy( r.data() + n, r2.data(), r.size() - n );
            break;
        }

        boost::hash2::detail::memcpy( r.data() + n, r2.data(), r2.size() );
        n += r2.size();
    }

    return r;
}

template<class H, std::size_t D, std::size_t N> BOOST_CXX14_CONSTEXPR boost::hash2::digest<D> xof_digest( char const (&str)[ N ] )
{
    H h;

    std::size_t const M = N - 1; // strip off null-terminator

    unsigned char buf[ N ] = {}; // avoids zero-sized arrays
    for( unsigned i = 0; i < M; ++i ){ buf[i] = str[i]; }

    h.update( buf, M );

    return get_digest_result<D>( h );
}

int main()
{
    using namespace boost::hash2;

    {
        constexpr char const str1[] = "abc";
        constexpr char const str2[] = "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu";

        TEST_EQ( test<shake_128>( 0, str1 ), digest_from_hex( "5881092dd818bf5cf8a3ddb793fbcba74097d5c526a6d35f97b83351940f2cc844c50af32acd3f2cdd066568706f509bc1bdde58295dae3f891a9a0fca5783789a41f8611214ce612394df286a62d1a2252aa94db9c538956c717dc2bed4f232a0294c857c730aa16067ac1062f1201fb0d377cfb9cde4c63599b27f3462bba4a0ed296c801f9ff7f57302bb3076ee145f97a32ae68e76ab66c48d51675bd49acc29082f5647584e" ) );
        TEST_EQ( test<shake_128>( 0, str2 ), digest_from_hex( "7b6df6ff181173b6d7898d7ff63fb07b7c237daf471a5ae5602adbccef9ccf4b37e06b4a3543164ffbe0d0557c02f9b25ad434005526d88ca04a6094b93ee57a55d5ea66e744bd391f8f52baf4e031d9e60e5ca32a0ed162bb89fc908097984548796652952dd4737d2a234a401f4857f3d1866efa736fd6a8f7c0b5d02ab06e5f821b2cc8cb8b4606fb15b9527cce5c3ec02c65cd1cdb5c81bd67686ebdd3b5b3fcffb123ca8ca6" ) );

        TEST_EQ( test<shake_128>( 7, str1 ), digest_from_hex( "502717a8480a3cb9e2a151854f3cdce6cb301227a4e0e37e03ee760454ad20d9acae1fa8939ca67210b0f817483636a9f5d78173cf3c121d59e2f83f04a5fe91bac770a724e838c3fbac03e753cbf64d3455bed7b5e355aa082b8829da3d3b77d5ef004c0922067607530895703a17cd34b0fc7dc330ea132bddf9d8b9878cd9f9c0a1d86bfac05ab0cbc6d97ac4c299655b3ce0aa0f4a9ba18efa28cf526dc443a959c9b950df5f" ) );
        TEST_EQ( test<shake_128>( 7, str2 ), digest_from_hex( "4df5c9ffeeb6864010e611151b5d1b6a67804cde351ea270c0a33debf5d2d3c03d9e23b69e642b7890bf809d181b97c138b6f86ca597fd675f2f81fb452e44445c25bccb0cb115ea596925088dba2c4810ae69cdcf1d9886adf759d80498792bc4ac2c2dae5b6dcf992b634387092d027845389b8a1b3cdc76678c35da210b4d525f1f9fa7d321dc413da7c37b6fd98405517cc71d7d759bd5746619df2cdb429e0d9842d8603e8f" ) );
    }

    {
        constexpr auto const N = shake_128::block_size;

        constexpr char buf1[] = "";
        constexpr unsigned char buf2[ N - 1 ] = {};
        constexpr unsigned char buf3[ N ] = {};
        constexpr unsigned char buf4[ N + 1 ] = {};

        TEST_EQ( test<shake_128>( 0, buf1 ), digest_from_hex( "7f9c2ba4e88f827d616045507605853ed73b8093f6efbc88eb1a6eacfa66ef263cb1eea988004b93103cfb0aeefd2a686e01fa4a58e8a3639ca8a1e3f9ae57e235b8cc873c23dc62b8d260169afa2f75ab916a58d974918835d25e6a435085b2badfd6dfaac359a5efbb7bcc4b59d538df9a04302e10c8bc1cbf1a0b3a5120ea17cda7cfad765f5623474d368ccca8af0007cd9f5e4c849f167a580b14aabdefaee7eef47cb0fca9" ) );
        TEST_EQ( test<shake_128>( 0, buf2 ), digest_from_hex( "959c3093774a513e807a36f3b23e508c10a5d78cc387266b5676ccbfbacc244f3bd2ae6948a948a6590fd78db4f154bda280de8e35a04bb8f59c9f620140e5384f76b626dbf6acd53ed84deb8d3261096fc9d6102ee90879e8f528118dbcb5f71c67c47cd2a55a47748bd7d905ac1891d044814255a7fe6b9f345fb32fb5693fa610d314f2bce5cf202c80fb59e3f7f6ef42e50b44d95aab96141d3407a4d2b2631171176eeb3b52" ) );
        TEST_EQ( test<shake_128>( 0, buf3 ), digest_from_hex( "7c00ff4748870cb26da4dc078aff74477ab153fa1191c7b636fea6c01ecc1fabff69544877e0279acfa01dd839d904dd0caa09c501056893e5ff4f9a6f525e07d0091cfa449c4327664e12425cf286b36ebe260b56bb2a3017bf7861063e5129a58b90fcf01c136e567a93f276d81a7bcf9cefc95c5ea5d8a45862c1e8867f0ed6848e70dd2fec1e89b4d07e1eac2a39e2ed7a5653bf54aff2c7b908f3042bc593eea55755094f17" ) );
        TEST_EQ( test<shake_128>( 0, buf4 ), digest_from_hex( "7dbf2395341028d86a561234f3fd598159b9307e5fabedfaeb9caab25d3bcc9a5fb896bfb8597ae96a711bf7f1ae9e74885f449cacb750b73d3686c018167e042c744224675890c744373e78993c53350dd4baf8f00da1b940789f67e8fc6a9ae7fcd379021d2564165cd1f59d3c57203d08c0096f3716c06d82bb93082bd5e897d18770a3996b0b9a393cf878d5eedef6412f0be017f82b0e377a59cb0836e2e0a630c3de90d554" ) );

        TEST_EQ( test<shake_128>( 7, buf1 ), digest_from_hex( "bf8adabca307e17e50091d99b33f214755ccdb30274c10c32b6e83a0ee90d3e69e1b750ea5cb332dc41dd4b122c526f965a982e59ee0130e4f668db419f09b5cd16abd11a91bb655e2a4738860843ab897d1d52cccdffb730b0a023802bb579beb6239c1ce25767f6b8de6e0f687a7afab7a2ade5efb0d2b9111baedfd9e795c8b8adb0f1b1bbf03fbd2ad1e6736055bbaaff96a65eb60a24916d66c577cb367198f1ae40a657744" ) );
        TEST_EQ( test<shake_128>( 7, buf2 ), digest_from_hex( "3f2e9d8b74326e3b725d9dcdcf13e8c1000b3545777e09ad92de51ce17741acde10f95283aa5b4d1b45cac3e7758bc4462992568580d76fdfa0ce39e660e0d8f7a1b3a64e554601b1e7519febe3f4ce861482227b211014832bf4ca15af31b272624a506db6a8146fab01af05de62cac967b8af80ce216ff6ce1bf45f34d35c3ccc08600afc2ca163ad21f146be7f186d3700beb03dc292e8ea0e8ba8f7fc8250442c178117c81ff" ) );
#if !BOOST_WORKAROUND(BOOST_MSVC, < 1920)
        TEST_EQ( test<shake_128>( 7, buf3 ), digest_from_hex( "34edff74b10937060a80329c4ef3307d8c9ee18df23f00637990dbd50fa0494dd104285c234ee27e0454832fc25a346243f991ca41b70eded3c135c799d4a8237bce127df7d42098ce232b368c1a37fed375222b1cccde2b2b28eb7243fefed18816eaa2cd50e8d20756ba0b12f8038bfe9773e914a58050ee2162f46097ce0c121cbe4f49131fd011ca6076a5872e56e0385418cd26732f836278419485465d1c2e23164546ed31" ) );
        TEST_EQ( test<shake_128>( 7, buf4 ), digest_from_hex( "29b564d049fff5fc7863eb5b4aae7d12cb39561f617f9d9bde23816258a968df0ea60de68dc81ebbf366b2b6b096432e9c8b7c5187735d48161ec5f16ee933496c630fb92f080c2282a3ceeffb30b208a5f79567bd204e283e2dc45853d9d7e64890382e4223a358795503b43db78d67198ba140aa4bd366f879114483f3581a03ee81afba75c932a06f42788e784eb4e9a8e5686ddee065c058ef3c53fddc2405a2b2b5eb876331" ) );
#endif
    }

    {
#if !BOOST_WORKAROUND(BOOST_MSVC, < 1920)
        TEST_EQ( (xof_digest<shake_128, 512>( "" )), digest_from_hex( "7f9c2ba4e88f827d616045507605853ed73b8093f6efbc88eb1a6eacfa66ef263cb1eea988004b93103cfb0aeefd2a686e01fa4a58e8a3639ca8a1e3f9ae57e235b8cc873c23dc62b8d260169afa2f75ab916a58d974918835d25e6a435085b2badfd6dfaac359a5efbb7bcc4b59d538df9a04302e10c8bc1cbf1a0b3a5120ea17cda7cfad765f5623474d368ccca8af0007cd9f5e4c849f167a580b14aabdefaee7eef47cb0fca9767be1fda69419dfb927e9df07348b196691abaeb580b32def58538b8d23f87732ea63b02b4fa0f4873360e2841928cd60dd4cee8cc0d4c922a96188d032675c8ac850933c7aff1533b94c834adbb69c6115bad4692d8619f90b0cdf8a7b9c264029ac185b70b83f2801f2f4b3f70c593ea3aeeb613a7f1b1de33fd75081f592305f2e4526edc09631b10958f464d889f31ba010250fda7f1368ec2967fc84ef2ae9aff268e0b1700affc6820b523a3d917135f2dff2ee06bfe72b3124721d4a26c04e53a75e30e73a7a9c4a95d91c55d495e9f51dd0b5e9d83c6d5e8ce803aa62b8d654db53d09b8dcff273cdfeb573fad8bcd45578bec2e770d01efde86e721a3f7c6cce275dabe6e2143f1af18da7efddc4c7b70b5e345db93cc936bea323491ccb38a388f546a9ff00dd4e1300b9b2153d2041d205b443e41b45a653f2a5c4492c1add544512dda2529833462b71a41a45be97290b6f" ) );
#endif
    }

    return boost::report_errors();
}
