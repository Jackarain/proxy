// Copyright 2017 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/fnv1a.hpp>
#include <boost/core/lightweight_test.hpp>
#include <cstring>

template<class H, class R> void test( char const * s, R r )
{
    H h;

    h.update( s, std::strlen( s ) );

    BOOST_TEST_EQ( h.result(), r );
}

int main()
{
    // Test vectors from https://tools.ietf.org/html/draft-eastlake-fnv-14

    test<boost::hash2::fnv1a_32>( "", 0x811c9dc5ul );
    test<boost::hash2::fnv1a_32>( "a", 0xe40c292cul );
    test<boost::hash2::fnv1a_32>( "foobar", 0xbf9cf968ul );

    test<boost::hash2::fnv1a_64>( "", 0xcbf29ce484222325ull );
    test<boost::hash2::fnv1a_64>( "a", 0xaf63dc4c8601ec8cull );
    test<boost::hash2::fnv1a_64>( "foobar", 0x85944171f73967e8ull );

    return boost::report_errors();
}
