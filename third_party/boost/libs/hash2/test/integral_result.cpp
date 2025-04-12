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
#include <boost/hash2/get_integral_result.hpp>
#include <boost/core/lightweight_test.hpp>
#include <cstdint>
#include <algorithm>
#include <cstddef>

using boost::hash2::get_integral_result;

template<class H> void test()
{
    H h;

    get_integral_result<int>( h );
    get_integral_result<std::uint32_t>( h );
    get_integral_result<std::uint64_t>( h );
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
