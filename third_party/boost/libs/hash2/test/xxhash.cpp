// Copyright 2017, 2018 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/xxhash.hpp>
#include <boost/core/lightweight_test.hpp>
#include <cstring>

template<class H, class S> typename H::result_type hash( char const * s, S seed )
{
    H h( seed );

    h.update( s, std::strlen( s ) );

    return h.result();
}

int main()
{
    // Test vectors from https://pixinsight.com/developer/pcl/doc/html/group__hash__functions.html

    BOOST_TEST_EQ( hash<boost::hash2::xxhash_32>( "", 0 ), 0x2cc5d05u );
    BOOST_TEST_EQ( hash<boost::hash2::xxhash_32>( "The quick brown fox jumps over the lazy dog", 43 ), 0x752cd1b8u );

    BOOST_TEST_EQ( hash<boost::hash2::xxhash_64>( "", 0 ), 0xef46db3751d8e999ull );
    BOOST_TEST_EQ( hash<boost::hash2::xxhash_64>( "The quick brown fox jumps over the lazy dog", 43 ), 0x9a11f5e9468d7425ull );

    // Test vectors from https://github.com/nashby/xxhash/blob/master/test/xxhash_test.rb

    BOOST_TEST_EQ( hash<boost::hash2::xxhash_32>( "test", 0 ), 1042293711u );
    BOOST_TEST_EQ( hash<boost::hash2::xxhash_32>( "test", 123 ), 2758658570u );

    BOOST_TEST_EQ( hash<boost::hash2::xxhash_64>( "test", 0 ), 5754696928334414137ull );
    BOOST_TEST_EQ( hash<boost::hash2::xxhash_64>( "test", 123 ), 3134990500624303823ull );

    return boost::report_errors();
}
