// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/hash_append.hpp>
#include <boost/hash2/fnv1a.hpp>
#include <boost/core/lightweight_test.hpp>

int main()
{
    unsigned const w[ 4 ] = { 1, 2, 3, 4 };

    boost::hash2::default_flavor f;

    {
        boost::hash2::hash_append_provider pr;

        boost::hash2::fnv1a_32 h1;
        boost::hash2::hash_append( h1, f, w );

        boost::hash2::fnv1a_32 h2;
        pr.hash_append( h2, f, w );

        BOOST_TEST_EQ( h1.result(), h2.result() );

        (void)pr; // avoid msvc-14.0 warning
    }

    {
        boost::hash2::hash_append_provider pr;

        boost::hash2::fnv1a_32 h1;
        boost::hash2::hash_append_range( h1, f, w + 0, w + 4 );

        boost::hash2::fnv1a_32 h2;
        pr.hash_append_range( h2, f, w + 0, w + 4 );

        BOOST_TEST_EQ( h1.result(), h2.result() );

        (void)pr; // avoid msvc-14.0 warning
    }

    {
        boost::hash2::hash_append_provider pr;

        boost::hash2::fnv1a_32 h1;
        boost::hash2::hash_append_size( h1, f, 4 );

        boost::hash2::fnv1a_32 h2;
        pr.hash_append_size( h2, f, 4 );

        BOOST_TEST_EQ( h1.result(), h2.result() );

        (void)pr; // avoid msvc-14.0 warning
    }

    {
        boost::hash2::hash_append_provider pr;

        boost::hash2::fnv1a_32 h1;
        boost::hash2::hash_append_range_and_size( h1, f, w + 0, w + 4 );

        boost::hash2::fnv1a_32 h2;
        pr.hash_append_range_and_size( h2, f, w + 0, w + 4 );

        BOOST_TEST_EQ( h1.result(), h2.result() );

        (void)pr; // avoid msvc-14.0 warning
    }

    {
        boost::hash2::hash_append_provider pr;

        boost::hash2::fnv1a_32 h1;
        boost::hash2::hash_append_unordered_range( h1, f, w + 0, w + 4 );

        boost::hash2::fnv1a_32 h2;
        pr.hash_append_unordered_range( h2, f, w + 0, w + 4 );

        BOOST_TEST_EQ( h1.result(), h2.result() );

        (void)pr; // avoid msvc-14.0 warning
    }

    return boost::report_errors();
}
