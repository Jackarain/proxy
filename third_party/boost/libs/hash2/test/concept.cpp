// Copyright 2017, 2018 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/fnv1a.hpp>
#include <boost/hash2/siphash.hpp>
#include <boost/hash2/xxhash.hpp>
#include <boost/hash2/md5.hpp>
#include <boost/hash2/sha1.hpp>
#include <boost/hash2/sha2.hpp>
#include <boost/hash2/sha3.hpp>
#include <boost/hash2/ripemd.hpp>
#include <boost/hash2/digest.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <array>
#include <type_traits>
#include <limits>
#include <cstddef>
#include <cstdint>

template<class R> struct is_valid_result:
    std::integral_constant<bool,
        std::is_integral<R>::value && !std::is_signed<R>::value
    >
{
};

template<std::size_t N> struct is_valid_result< std::array<unsigned char, N> >:
    std::integral_constant<bool, N >= 8>
{
};

template<std::size_t N> struct is_valid_result< boost::hash2::digest<N> >:
    std::integral_constant<bool, N >= 8>
{
};

template<class H> void test_result_type()
{
    BOOST_TEST_TRAIT_TRUE((is_valid_result<typename H::result_type>));
}

template<class H> void test_default_constructible()
{
    H h1;
    typename H::result_type r1 = h1.result();

    H h2;
    typename H::result_type r2 = h2.result();

    BOOST_TEST( r1 == r2 );

    H h3;
    h3.update( static_cast<void const*>( nullptr ), 0 );
    typename H::result_type r3 = h3.result();

    BOOST_TEST( r1 == r3 );
}

template<class H> void test_byte_seed_constructible( bool is_hmac )
{
    {
        unsigned char const seed[ 3 ] = { 0x01, 0x02, 0x03 };

        {
            H h1;
            H h2( static_cast<void const*>( nullptr ), 0 );

            BOOST_TEST( h1.result() == h2.result() );
        }

        {
            H h1;
            H h2( seed, 0 );

            BOOST_TEST( h1.result() == h2.result() );
        }

        {
            H h1;
            H h2( seed, 3 );

            BOOST_TEST( h1.result() != h2.result() );
        }

        {
            H h1( seed, 3 );
            H h2( seed, 3 );

            BOOST_TEST( h1.result() == h2.result() );
        }

        {
            H h1( seed, 2 );
            H h2( seed, 3 );

            BOOST_TEST( h1.result() != h2.result() );
        }

        {
            H h1( seed, 3 );

            H h2( seed, 3 );
            h2.update( seed, 0 );

            BOOST_TEST( h1.result() == h2.result() );
        }

        {
            H h1( seed, 3 );

            H h2( seed, 0 );
            h2.update( seed, 3 );

            BOOST_TEST( h1.result() != h2.result() );
        }

        {
            H h1( seed, 3 );

            H h2( seed, 2 );
            h2.update( seed + 2, 1 );

            BOOST_TEST( h1.result() != h2.result() );
        }

        {
            char const seed2[] = { 0x01, 0x02, 0x03 };

            H h1( seed, 3 );
            H h2( seed2, 3 );

            BOOST_TEST( h1.result() == h2.result() );
        }
    }

    if( !is_hmac )
    {
        // RFC 2104 mandates null padding of short keys,
        // so these tests are required to fail for hmac_xxx

        unsigned char const seed[ 3 ] = { 0x00, 0x00, 0x00 };

        {
            H h1;
            H h2( seed, 3 );

            BOOST_TEST( h1.result() != h2.result() );
        }

        {
            H h1( seed, 2 );
            H h2( seed, 3 );

            BOOST_TEST( h1.result() != h2.result() );
        }
    }
}

template<class H> void test_integral_seed_constructible()
{
    typename H::result_type r0, r1, r2, r3;

    {
        H h( 0 );
        r0 = h.result();
    }

    {
        H h( 1 );
        r1 = h.result();
    }

    BOOST_TEST( r0 != r1 );

    {
        H h( 2 );
        r2 = h.result();
    }

    BOOST_TEST( r0 != r2 );
    BOOST_TEST( r1 != r2 );

    {
        H h( 1LL << 32 );
        r3 = h.result();
    }

    BOOST_TEST( r0 != r3 );
    BOOST_TEST( r1 != r3 );
    BOOST_TEST( r2 != r3 );

    {
        H h;
        BOOST_TEST( h.result() == r0 );
    }

    {
        H h( 0 );
        h.update( static_cast<void const*>( nullptr ), 0 );

        BOOST_TEST( h.result() == r0 );
    }

    {
        H h( 0L );
        BOOST_TEST( h.result() == r0 );
    }

    {
        H h( 1L );
        BOOST_TEST( h.result() == r1 );
    }

    {
        H h( 2L );
        BOOST_TEST( h.result() == r2 );
    }

    {
        H h( 0LL );
        BOOST_TEST( h.result() == r0 );
    }

    {
        H h( 1LL );
        BOOST_TEST( h.result() == r1 );
    }

    {
        H h( 2LL );
        BOOST_TEST( h.result() == r2 );
    }

    {
        H h( static_cast<std::uint32_t>( 0 ) );
        BOOST_TEST( h.result() == r0 );
    }

    {
        H h( static_cast<std::uint32_t>( 1 ) );
        BOOST_TEST( h.result() == r1 );
    }

    {
        H h( static_cast<std::uint32_t>( 2 ) );
        BOOST_TEST( h.result() == r2 );
    }

    {
        H h( static_cast<std::size_t>( 0 ) );
        BOOST_TEST( h.result() == r0 );
    }

    {
        H h( static_cast<std::size_t>( 1 ) );
        BOOST_TEST( h.result() == r1 );
    }

    {
        H h( static_cast<std::size_t>( 2 ) );
        BOOST_TEST( h.result() == r2 );
    }

    {
        H h( static_cast<std::uint64_t>( 0 ) );
        BOOST_TEST( h.result() == r0 );
    }

    {
        H h( static_cast<std::uint64_t>( 1 ) );
        BOOST_TEST( h.result() == r1 );
    }

    {
        H h( static_cast<std::uint64_t>( 2 ) );
        BOOST_TEST( h.result() == r2 );
    }

    {
        H h( static_cast<std::uint64_t>( 1 ) << 32 );
        BOOST_TEST( h.result() == r3 );
    }
}

template<class H> void test_copy_constructible()
{
    unsigned char const seed[ 3 ] = { 0x01, 0x02, 0x03 };

    {
        H h1;
        H h2( h1 );

        BOOST_TEST( h1.result() == h2.result() );
    }

    {
        H h1( seed, 3 );
        H h2( h1 );

        BOOST_TEST( h1.result() == h2.result() );
    }

    {
        H h1;
        h1.update( seed, 3 );

        H h2( h1 );

        BOOST_TEST( h1.result() == h2.result() );
    }
}

template<class H> void test_update()
{
    {
        unsigned char const data[ 3 ] = { 0x01, 0x02, 0x03 };

        {
            H h1;

            H h2;
            h2.update( data, 3 );

            BOOST_TEST( h1.result() != h2.result() );
        }

        {
            H h1;
            h1.update( data, 3 );

            H h2;
            h2.update( data, 3 );

            BOOST_TEST( h1.result() == h2.result() );
        }

        {
            H h1;
            h1.update( data, 3 );

            H h2;
            h2.update( data, 2 );

            BOOST_TEST( h1.result() != h2.result() );
        }

        {
            H h1;
            h1.update( data, 3 );

            H h2( h1 );

            h1.update( data, 3 );
            h2.update( data, 3 );

            BOOST_TEST( h1.result() == h2.result() );
        }

        {
            H h1;
            h1.update( data, 3 );

            H h2;
            h2.update( data + 0, 1 );
            h2.update( data + 1, 1 );
            h2.update( data + 2, 1 );

            BOOST_TEST( h1.result() == h2.result() );
        }

        {
            H h1;
            h1.update( data, 3 );

            H h2;
            h2.update( data, 0 );
            h2.update( data + 0, 3 );

            BOOST_TEST( h1.result() == h2.result() );
        }

        {
            H h1;
            h1.update( data, 3 );

            H h2;
            h2.update( data, 1 );
            h2.update( data + 1, 2 );

            BOOST_TEST( h1.result() == h2.result() );
        }

        {
            H h1;
            h1.update( data, 3 );

            H h2;
            h2.update( data, 2 );
            h2.update( data + 2, 1 );

            BOOST_TEST( h1.result() == h2.result() );
        }

        {
            H h1;
            h1.update( data, 3 );

            H h2;
            h2.update( data, 3 );
            h2.update( data + 3, 0 );

            BOOST_TEST( h1.result() == h2.result() );
        }
    }

    {
        int const N = 255;

        unsigned char const data[ N ] = {};

        for( int i = 0; i <= N; ++i )
        {
            H h1;
            h1.update( data, N );

            H h2;

            h2.update( data, i );
            h2.update( data + i, N - i );

            BOOST_TEST( h1.result() == h2.result() );
        }

        {
            H h1;
            h1.update( data, N );

            H h2;

            for( int i = 0; i < N / 5; ++i )
            {
                h2.update( data + i * 5, 5 );
            }

            BOOST_TEST( h1.result() == h2.result() );
        }
    }
}

template<class H> void test_assignable()
{
    unsigned char const seed[ 3 ] = { 0x01, 0x02, 0x03 };

    {
        H h1;
        H h2( h1 );

        typename H::result_type r1 = h1.result();

        BOOST_TEST( h1.result() != r1 );

        h1 = h2;
        BOOST_TEST( h1.result() == r1 );
    }

    {
        H h1( seed, 3 );
        H h2( h1 );

        typename H::result_type r1 = h1.result();

        BOOST_TEST( h1.result() != r1 );

        h1 = h2;
        BOOST_TEST( h1.result() == r1 );
    }

    {
        H h1;
        h1.update( seed, 3 );

        H h2( h1 );

        typename H::result_type r1 = h1.result();

        BOOST_TEST( h1.result() != r1 );

        h1 = h2;
        BOOST_TEST( h1.result() == r1 );
    }
}

template<class H> void test( bool is_hmac = false )
{
    test_result_type<H>();
    test_default_constructible<H>();
    test_byte_seed_constructible<H>( is_hmac );
    test_integral_seed_constructible<H>();
    test_copy_constructible<H>();
    test_update<H>();
    test_assignable<H>();
}

int main()
{
    test<boost::hash2::fnv1a_32>();
    test<boost::hash2::fnv1a_64>();
    test<boost::hash2::xxhash_32>();
    test<boost::hash2::xxhash_64>();
    test<boost::hash2::siphash_32>();
    test<boost::hash2::siphash_64>();

    test<boost::hash2::md5_128>();
    test<boost::hash2::sha1_160>();
    test<boost::hash2::sha2_256>();
    test<boost::hash2::sha2_224>();
    test<boost::hash2::sha2_512>();
    test<boost::hash2::sha2_384>();
    test<boost::hash2::sha2_512_224>();
    test<boost::hash2::sha2_512_256>();
    test<boost::hash2::sha3_256>();
    test<boost::hash2::sha3_224>();
    test<boost::hash2::sha3_512>();
    test<boost::hash2::sha3_384>();
    test<boost::hash2::shake_128>();
    test<boost::hash2::shake_256>();
    test<boost::hash2::ripemd_160>();
    test<boost::hash2::ripemd_128>();

    test<boost::hash2::hmac_md5_128>( true );
    test<boost::hash2::hmac_sha1_160>( true );
    test<boost::hash2::hmac_sha2_256>( true );
    test<boost::hash2::hmac_sha2_224>( true );
    test<boost::hash2::hmac_sha2_512>( true );
    test<boost::hash2::hmac_sha2_384>( true );
    test<boost::hash2::hmac_sha2_512_224>( true );
    test<boost::hash2::hmac_sha2_512_256>( true );
    test<boost::hash2::hmac_sha3_256>( true );
    test<boost::hash2::hmac_sha3_224>( true );
    test<boost::hash2::hmac_sha3_512>( true );
    test<boost::hash2::hmac_sha3_384>( true );
    test<boost::hash2::hmac_ripemd_160>( true );
    test<boost::hash2::hmac_ripemd_128>( true );

    return boost::report_errors();
}
