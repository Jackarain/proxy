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

        TEST_EQ( test<shake_256>( 0, str1 ), digest_from_hex( "483366601360a8771c6863080cc4114d8db44530f8f1e1ee4f94ea37e78b5739d5a15bef186a5386c75744c0527e1faa9f8726e462a12a4feb06bd8801e751e41385141204f329979fd3047a13c5657724ada64d2470157b3cdc288620944d78dbcddbd912993f0913f164fb2ce95131a2d09a3e6d51cbfc622720d7a75c6334e8a2d7ec71a7cc29" ) );
        TEST_EQ( test<shake_256>( 0, str2 ), digest_from_hex( "98be04516c04cc73593fef3ed0352ea9f6443942d6950e29a372a681c3deaf4535423709b02843948684e029010badcc0acd8303fc85fdad3eabf4f78cae165635f57afd28810fc22abf63df55c5ead450fdfb64209010e982102aa0b5f0a4b4753b53eb4b5319c06986f5aac5cc247256d06b05a273d7ef8d31864777d488d541451ed82a389265" ) );

        TEST_EQ( test<shake_256>( 7, str1 ), digest_from_hex( "a98e0a6e0e0510cbde22f1bb953abdaec816ded1982f02dab9f7094c1ad56a62e2a1169ac6b540cee8c16d846245a9911cfba87a208f4317c46b9a2a6196bc401247c40eec30200480bd5afadccdd5007aa3d38315428ca704e72b045e2ceb60c8f0627e149396bfbcf25acb2e31b990f888b202bc4a48462e60df572feeafe75bc50ca468235774" ) );
        TEST_EQ( test<shake_256>( 7, str2 ), digest_from_hex( "3bafc7d2e320f1a1c3fa3de31322361a5df67347da3f3c4c4e739315de26b65d69a2c1a469851e3b9654355cb299908c2c8c5339d1e680a79de876898831042230d60c5a7b570a4e49774d7dcec6d4efc04b75eebbd6dc45bec25585220dbaecbe279d40a9e0604f9bfa6842348a192bb572faafd78ccb0a91e0afda9c7842ad8ccc2c08192865aa" ) );
    }

    {
        constexpr auto const N = shake_256::block_size;

        constexpr char buf1[] = "";
        constexpr unsigned char buf2[ N - 1 ] = {};
        constexpr unsigned char buf3[ N ] = {};
        constexpr unsigned char buf4[ N + 1 ] = {};

        TEST_EQ( test<shake_256>( 0, buf1 ), digest_from_hex( "46b9dd2b0ba88d13233b3feb743eeb243fcd52ea62b81b82b50c27646ed5762fd75dc4ddd8c0f200cb05019d67b592f6fc821c49479ab48640292eacb3b7c4be141e96616fb13957692cc7edd0b45ae3dc07223c8e92937bef84bc0eab862853349ec75546f58fb7c2775c38462c5010d846c185c15111e595522a6bcd16cf86f3d122109e3b1fdd" ) );
        TEST_EQ( test<shake_256>( 0, buf2 ), digest_from_hex( "4a6c0970c326babfaeef17f91988d1b4c5e95ed584c21b55b9f92e0d3671ddf98ec3e9ba8d1ac5c546a27662e979464ae5e56c58b9a3a1460929176efc35a49c2d2af9882e866f5c1af9d07dd976eac27242be661511bb0b7918b2008bfb128171a608b0bd4dcb181bd106248daba5f368c0c23e31c7d07ee61701bbeedccb1b16d393a538b50544" ) );
        TEST_EQ( test<shake_256>( 0, buf3 ), digest_from_hex( "ea947b835fec1f9b0a7eabba901deb7881fd9999a1cbd5ccbb5a9afab7f6fe70d85dc53e04c61e86e1f32a3162d2ea9ae4812e6119ce4556ccbfede11c3a0cfbc66b07e04befa18c4ac4f15d1212ab01f0419fc914740040b46b247b88d0e21d3b9e5dfb73383414a82e918460fa8031dca813dcc20b35323e151249b590e732d9cc8933d736c521" ) );
        TEST_EQ( test<shake_256>( 0, buf4 ), digest_from_hex( "60691a6b6b79c4abf99438b3f7a6455f2ce44fed8c8546cc90c218fe37ba546621f6a79cb859e9a6c69cc280bd9b4c7d07d30c7afa5069ffb6be6f42f110b071b6924b6c9d3f7b511f0a28424bf665ddfb2ccb7ce6633edc2b956a43332d292923b5ad4d0047852cb93e252606e9b4d8721bb7e2348019b7f41f509c4737030a84b19f44b0b3711b" ) );

        TEST_EQ( test<shake_256>( 7, buf1 ), digest_from_hex( "e62844bdb20e510107fa2b1c2d79a5e899c358c53d1753f85e4d85d52ad0f57697cf1b45ed5e6808f44d85fb1c6a0c7fe0bc4c618bbca8640234ca1ac9ed37b02159f3c70458454c5e89db70c8a29a16708a8f495ede9414820cc91643b04f7f3fa2e522b0554fcde9401550b1fd5024da6ce884691a73110b8cb251e61df1d72616e59bef4ebccb" ) );
        TEST_EQ( test<shake_256>( 7, buf2 ), digest_from_hex( "b3918c624df8f56419fe3cbae1d996fa75e3fc8f292cd4bd70c110bb8ce2191606415b37c2847472af248a00250f1774fc0e43a74818371ac8610ce9097d25635297c6d758905f5a7bafb184e08edb5d282c6eaed770d51ccc83a35be725553102bd2b67af5c7dda4b028938eae709ab3b2129c2b481de62094f1d0a4f375231f50b2544fe4d9c7d" ) );
#if !BOOST_WORKAROUND(BOOST_MSVC, < 1920)
        TEST_EQ( test<shake_256>( 7, buf3 ), digest_from_hex( "3cc8e5553bb37d4fd552ff699212f79c597704aab510af80a6527a1c151c9c3c99deb695524a0a4767e37c598a0bfc5a99d0dad569fa54b0169547e64dbb912ad566e822f626412722d2295ddc04a05af9cdf13fa091d08b0832a97f3a7ce504bafb76736a039618db37b7600b6892edd003e575f04ad6cea5159d8d1bb79514126e8fec495c3692" ) );
        TEST_EQ( test<shake_256>( 7, buf4 ), digest_from_hex( "2a99baeddb323f389ff20a2b9fb2a17b9b8840a3c085126ffd4825af013e6237e3b8527a5697e7ae0cb4018019501cf344bb8dea38b1e3cc4f43f488f864779f12a6f537667e870383eaea85eb9b17f3f69be152d388dc24777ce0adaaf27a37892f02c5bb6b42a577d354a414625dc0f613dac0b99206125ed969ae9aa76190c1bda8b0141cfc07" ) );
#endif
    }

    {
#if !BOOST_WORKAROUND(BOOST_MSVC, < 1920)
        TEST_EQ( (xof_digest<shake_256, 512>( "" )), digest_from_hex( "46b9dd2b0ba88d13233b3feb743eeb243fcd52ea62b81b82b50c27646ed5762fd75dc4ddd8c0f200cb05019d67b592f6fc821c49479ab48640292eacb3b7c4be141e96616fb13957692cc7edd0b45ae3dc07223c8e92937bef84bc0eab862853349ec75546f58fb7c2775c38462c5010d846c185c15111e595522a6bcd16cf86f3d122109e3b1fdd943b6aec468a2d621a7c06c6a957c62b54dafc3be87567d677231395f6147293b68ceab7a9e0c58d864e8efde4e1b9a46cbe854713672f5caaae314ed9083dab4b099f8e300f01b8650f1f4b1d8fcf3f3cb53fb8e9eb2ea203bdc970f50ae55428a91f7f53ac266b28419c3778a15fd248d339ede785fb7f5a1aaa96d313eacc890936c173cdcd0fab882c45755feb3aed96d477ff96390bf9a66d1368b208e21f7c10d04a3dbd4e360633e5db4b602601c14cea737db3dcf722632cc77851cbdde2aaf0a33a07b373445df490cc8fc1e4160ff118378f11f0477de055a81a9eda57a4a2cfb0c83929d310912f729ec6cfa36c6ac6a75837143045d791cc85eff5b21932f23861bcf23a52b5da67eaf7baae0f5fb1369db78f3ac45f8c4ac5671d85735cdddb09d2b1e34a1fc066ff4a162cb263d6541274ae2fcc865f618abe27c124cd8b074ccd516301b91875824d09958f341ef274bdab0bae316339894304e35877b0c28a9b1fd166c796b9cc258a064a8f57e27f2a" ) );
#endif
    }

    return boost::report_errors();
}
