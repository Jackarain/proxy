// Copyright 2017, 2018 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/hash_append.hpp>
#include <boost/hash2/fnv1a.hpp>
#include <boost/core/lightweight_test.hpp>
#include <set>
#include <unordered_set>

template<class Hash, class Flavor, class Set, class R> void test( R r )
{
    {
        Set s;

        for( int i = 0; i < 64; ++i )
        {
            s.insert( i );
        }

        Hash h;
        Flavor f;

        hash_append( h, f, s );

        BOOST_TEST_EQ( h.result(), r );
    }

    {
        Set s;

        for( int i = 0; i < 64; ++i )
        {
            s.insert( 63 - i );
        }

        Hash h;
        Flavor f;

        hash_append( h, f, s );

        BOOST_TEST_EQ( h.result(), r );
    }

    {
        Set s;

        for( int i = 0; i < 64; ++i )
        {
            s.insert( ( i * 17 ) % 64 );
        }

        Hash h;
        Flavor f;

        hash_append( h, f, s );

        BOOST_TEST_EQ( h.result(), r );
    }
}

int main()
{
    using namespace boost::hash2;

    test< fnv1a_32, little_endian_flavor, std::set<int> >( 2078558933ul );
    test< fnv1a_64, little_endian_flavor, std::set<int> >( 6271229243378528309ull );

    test< fnv1a_32, little_endian_flavor, std::multiset<int> >( 2078558933ul );
    test< fnv1a_64, little_endian_flavor, std::multiset<int> >( 6271229243378528309ull );

    test< fnv1a_32, little_endian_flavor, std::unordered_set<int> >( 396645525ul );
    test< fnv1a_64, little_endian_flavor, std::unordered_set<int> >( 2830007867253608057ull );

    test< fnv1a_32, little_endian_flavor, std::unordered_multiset<int> >( 396645525ul );
    test< fnv1a_64, little_endian_flavor, std::unordered_multiset<int> >( 2830007867253608057ull );

    return boost::report_errors();
}
