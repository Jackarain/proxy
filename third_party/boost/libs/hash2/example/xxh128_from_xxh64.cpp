// Copyright 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/xxhash.hpp>
#include <boost/hash2/digest.hpp>
#include <boost/endian/conversion.hpp>

class xxhash_128: private boost::hash2::xxhash_64
{
public:

    using result_type = boost::hash2::digest<16>;

    using xxhash_64::xxhash_64;
    using xxhash_64::update;

    result_type result()
    {
        std::uint64_t r1 = xxhash_64::result();
        std::uint64_t r2 = xxhash_64::result();

        result_type r = {};

        boost::endian::store_little_u64( r.data() + 0, r1 );
        boost::endian::store_little_u64( r.data() + 8, r2 );

        return r;
    }
};

#include <string>
#include <iostream>

int main()
{
    std::string tv( "The quick brown fox jumps over the lazy dog" );

    xxhash_128 hash( 43 );
    hash.update( tv.data(), tv.size() );

    std::cout << hash.result() << std::endl;
}
