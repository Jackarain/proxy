// Copyright 2017 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// Endian-dependent test

#include <boost/hash2/hash_append.hpp>
#include <boost/hash2/fnv1a.hpp>
#include <boost/core/lightweight_test.hpp>
#include <vector>
#include <list>
#include <deque>
#include <set>
#include <forward_list>

template<class Hash, class Flavor, class T, class R> void test_unsized_range( R r )
{
    int w[] = { 1, 2, 3, 4 };
    T v( w, w + sizeof(w) / sizeof(w[0]) );

    Hash h;
    Flavor f;

    boost::hash2::hash_append_range( h, f, v.begin(), v.end() );

    BOOST_TEST_EQ( h.result(), r );
}

template<class Hash, class Flavor, class R> void test( R r )
{
    test_unsized_range< Hash, Flavor, std::vector<int> >( r );
    test_unsized_range< Hash, Flavor, std::list<int> >( r );
    test_unsized_range< Hash, Flavor, std::deque<int> >( r );
    test_unsized_range< Hash, Flavor, std::set<int> >( r );
    test_unsized_range< Hash, Flavor, std::multiset<int> >( r );
    test_unsized_range< Hash, Flavor, std::forward_list<int> >( r );

    {
        int v[2][2] = { { 1, 2 }, { 3, 4 } };

        Hash h;
        Flavor f;

        boost::hash2::hash_append_range( h, f, v, v + 2 );

        BOOST_TEST_EQ( h.result(), r );
    }
}

int main()
{
    using namespace boost::hash2;

    test<fnv1a_32, default_flavor>( 1041505217ul );
    test<fnv1a_64, default_flavor>( 9566659391000707361ull );

    return boost::report_errors();
}
