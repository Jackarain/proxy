// Copyright 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/hash_append.hpp>
#include <boost/hash2/fnv1a.hpp>
#include <boost/core/lightweight_test.hpp>
#include <vector>

int main()
{
    using namespace boost::hash2;

    {
        fnv1a_32 h;
        hash_append( h, {}, false );

        BOOST_TEST_EQ( h.result(), 84696351 );
    }

    {
        fnv1a_32 h;
        hash_append( h, {}, true );

        BOOST_TEST_EQ( h.result(), 67918732 );
    }

    {
        bool const v[ 7 ] = {};

        fnv1a_32 h;
        hash_append_range( h, {}, v + 0, v + 7 );

        BOOST_TEST_EQ( h.result(), 1177189415 );
    }

    {
        bool v[ 7 ] = {};

        fnv1a_32 h;
        hash_append_range( h, {}, v + 0, v + 7 );

        BOOST_TEST_EQ( h.result(), 1177189415 );
    }

    {
        std::vector<bool> v( 7 );

        fnv1a_32 h;
        hash_append_range( h, {}, v.begin(), v.end() );

        BOOST_TEST_EQ( h.result(), 1177189415 );
    }

    {
        std::vector<bool> const v( 7 );

        fnv1a_32 h;
        hash_append_range( h, {}, v.begin(), v.end() );

        BOOST_TEST_EQ( h.result(), 1177189415 );
    }

    return boost::report_errors();
}
