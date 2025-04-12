// Copyright 2017, 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/hash_append.hpp>
#include <boost/hash2/fnv1a.hpp>
#include <boost/core/lightweight_test.hpp>
#include <array>
#include <tuple>

template<class Hash, class Flavor, class T1, class T2> void test( T1 const& t1, T2 const& t2 )
{
    Flavor f;

    Hash h1;
    boost::hash2::hash_append( h1, f, t1 );

    Hash h2;
    boost::hash2::hash_append( h2, f, t2 );

    BOOST_TEST_EQ( h1.result(), h2.result() );
}

int main()
{
    using namespace boost::hash2;

    test<fnv1a_32, default_flavor>( std::tuple<>(), std::array<char, 0>() );
    test<fnv1a_32, default_flavor>( std::tuple<char>( '\x01' ), std::array<char, 1>{{ '\x01' }} );
    test<fnv1a_32, default_flavor>( std::tuple<char, char>( '\x01', '\x02' ), std::array<char, 2>{{ '\x01', '\x02' }} );
    test<fnv1a_32, default_flavor>( std::tuple<char, char, char>( '\x01', '\x02', '\x03' ), std::array<char, 3>{{ '\x01', '\x02', '\x03' }} );
    test<fnv1a_32, default_flavor>( std::tuple<char, char, char, char>( '\x01', '\x02', '\x03', '\x04' ), std::array<char, 4>{{ '\x01', '\x02', '\x03', '\x04' }} );
    test<fnv1a_32, default_flavor>( std::tuple<char, char, char, char, char>( '\x01', '\x02', '\x03', '\x04', '\x05' ), std::array<char, 5>{{ '\x01', '\x02', '\x03', '\x04', '\x05' }} );
    test<fnv1a_32, default_flavor>( std::tuple<char, char, char, char, char, char>( '\x01', '\x02', '\x03', '\x04', '\x05', '\x06' ), std::array<char, 6>{{ '\x01', '\x02', '\x03', '\x04', '\x05', '\x06' }} );

    test<fnv1a_32, little_endian_flavor>( std::tuple<>(), std::array<int, 0>() );
    test<fnv1a_32, little_endian_flavor>( std::tuple<int>( 1 ), std::array<int, 1>{{ 1 }} );
    test<fnv1a_32, little_endian_flavor>( std::tuple<int, int>( 1, 2 ), std::array<int, 2>{{ 1, 2 }} );
    test<fnv1a_32, little_endian_flavor>( std::tuple<int, int, int>( 1, 2, 3 ), std::array<int, 3>{{ 1, 2, 3 }} );
    test<fnv1a_32, little_endian_flavor>( std::tuple<int, int, int, int>( 1, 2, 3, 4 ), std::array<int, 4>{{ 1, 2, 3, 4 }} );
    test<fnv1a_32, little_endian_flavor>( std::tuple<int, int, int, int, int>( 1, 2, 3, 4, 5 ), std::array<int, 5>{{ 1, 2, 3, 4, 5 }} );
    test<fnv1a_32, little_endian_flavor>( std::tuple<int, int, int, int, int, int>( 1, 2, 3, 4, 5, 6 ), std::array<int, 6>{{ 1, 2, 3, 4, 5, 6 }} );

    return boost::report_errors();
}
