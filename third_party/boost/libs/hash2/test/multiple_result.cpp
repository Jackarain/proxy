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
#include <boost/core/lightweight_test.hpp>
#include <algorithm>
#include <cstddef>

template<class H> void test()
{
    {
        H h;

        typename H::result_type r1 = h.result();

        typename H::result_type r2 = h.result();
        BOOST_TEST( r2 != r1 );

        typename H::result_type r3 = h.result();
        BOOST_TEST( r3 != r2 );
        BOOST_TEST( r3 != r1 );

        typename H::result_type r4 = h.result();
        BOOST_TEST( r4 != r3 );
        BOOST_TEST( r4 != r2 );
        BOOST_TEST( r4 != r1 );
    }

    {
        H h1;
        H h2( 1 );

        BOOST_TEST( h1.result() != h2.result() );
        BOOST_TEST( h1.result() != h2.result() );
        BOOST_TEST( h1.result() != h2.result() );
        BOOST_TEST( h1.result() != h2.result() );
    }

    {
        H h1( 1 );
        H h2( 2 );

        BOOST_TEST( h1.result() != h2.result() );
        BOOST_TEST( h1.result() != h2.result() );
        BOOST_TEST( h1.result() != h2.result() );
        BOOST_TEST( h1.result() != h2.result() );
    }

    {
        unsigned char const seed[] = { 0x01, 0x02, 0x03 };

        H h1;
        H h2( seed, 3 );

        BOOST_TEST( h1.result() != h2.result() );
        BOOST_TEST( h1.result() != h2.result() );
        BOOST_TEST( h1.result() != h2.result() );
        BOOST_TEST( h1.result() != h2.result() );
    }

    {
        unsigned char const seed[] = { 0x01, 0x02, 0x03 };

        H h1( seed, 2 );
        H h2( seed, 3 );

        BOOST_TEST( h1.result() != h2.result() );
        BOOST_TEST( h1.result() != h2.result() );
        BOOST_TEST( h1.result() != h2.result() );
        BOOST_TEST( h1.result() != h2.result() );
    }

    {
        unsigned char const seq[] = { 0x01, 0x02, 0x03 };

        H h1; h1.update( seq, 2 );
        H h2; h2.update( seq, 3 );

        BOOST_TEST( h1.result() != h2.result() );
        BOOST_TEST( h1.result() != h2.result() );
        BOOST_TEST( h1.result() != h2.result() );
        BOOST_TEST( h1.result() != h2.result() );
    }
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

    test<boost::hash2::hmac_md5_128>();
    test<boost::hash2::hmac_sha1_160>();
    test<boost::hash2::hmac_sha2_256>();
    test<boost::hash2::hmac_sha2_224>();
    test<boost::hash2::hmac_sha2_512>();
    test<boost::hash2::hmac_sha2_384>();
    test<boost::hash2::hmac_sha2_512_224>();
    test<boost::hash2::hmac_sha2_512_256>();
    test<boost::hash2::hmac_sha3_256>();
    test<boost::hash2::hmac_sha3_224>();
    test<boost::hash2::hmac_sha3_512>();
    test<boost::hash2::hmac_sha3_384>();
    test<boost::hash2::hmac_ripemd_160>();
    test<boost::hash2::hmac_ripemd_128>();

    return boost::report_errors();
}
