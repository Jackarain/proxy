// Copyright 2017, 2018 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/hash_append.hpp>
#include <boost/hash2/fnv1a.hpp>
#include <boost/core/lightweight_test.hpp>
#include <map>
#include <unordered_map>

template<class Hash, class Flavor, class Map, class R> void test( R r )
{
    {
        Map m;

        for( int i = 0; i < 64; ++i )
        {
            m.insert( typename Map::value_type( i, 3 * i + 1 ) );
        }

        Hash h;
        Flavor f;

        hash_append( h, f, m );

        BOOST_TEST_EQ( h.result(), r );
    }

    {
        Map m;

        for( int i = 0; i < 64; ++i )
        {
            int j = 63 - i;
            m.insert( typename Map::value_type( j, 3 * j + 1 ) );
        }

        Hash h;
        Flavor f;

        hash_append( h, f, m );

        BOOST_TEST_EQ( h.result(), r );
    }

    {
        Map m;

        for( int i = 0; i < 64; ++i )
        {
            int j = ( i * 17 ) % 64;
            m.insert( typename Map::value_type( j, 3 * j + 1 ) );
        }

        Hash h;
        Flavor f;

        hash_append( h, f, m );

        BOOST_TEST_EQ( h.result(), r );
    }
}

int main()
{
    using namespace boost::hash2;

    test< fnv1a_32, little_endian_flavor, std::map<int, int> >( 3152726101ul );
    test< fnv1a_64, little_endian_flavor, std::map<int, int> >( 11386405661620022965ull );

    test< fnv1a_32, little_endian_flavor, std::multimap<int, int> >( 3152726101ul );
    test< fnv1a_64, little_endian_flavor, std::multimap<int, int> >( 11386405661620022965ull );

    test< fnv1a_32, little_endian_flavor, std::unordered_map<int, int> >( 3137657678ul );
    test< fnv1a_64, little_endian_flavor, std::unordered_map<int, int> >( 7026041901235387186ull );

    test< fnv1a_32, little_endian_flavor, std::unordered_multimap<int, int> >( 3137657678ul );
    test< fnv1a_64, little_endian_flavor, std::unordered_multimap<int, int> >( 7026041901235387186ull );

    return boost::report_errors();
}
