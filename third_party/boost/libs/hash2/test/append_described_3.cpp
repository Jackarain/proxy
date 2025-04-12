// Copyright 2024 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/hash_append.hpp>
#include <boost/hash2/fnv1a.hpp>
#include <boost/describe/class.hpp>
#include <boost/config/pragma_message.hpp>

#if !defined(BOOST_DESCRIBE_CXX14)

BOOST_PRAGMA_MESSAGE( "Test skipped, because BOOST_DESCRIBE_CXX14 is not defined" )
int main() {}

#else

#include <boost/core/lightweight_test.hpp>

struct X
{
};

BOOST_DESCRIBE_STRUCT(X, (), ())

template<class Hash, class Flavor> void test()
{
    typename Hash::result_type r1, r2, r3;

    Flavor f;

    {
        X x[ 1 ] = {};

        Hash h;
        hash_append( h, f, x );

        r1 = h.result();
    }

    {
        X x[ 2 ] = {};

        Hash h;
        hash_append( h, f, x );

        r2 = h.result();
    }

    BOOST_TEST_NE( r2, r1 );

    {
        X x[ 3 ] = {};

        Hash h;
        hash_append( h, f, x );

        r3 = h.result();
    }

    BOOST_TEST_NE( r3, r1 );
    BOOST_TEST_NE( r3, r2 );
}

int main()
{
    using namespace boost::hash2;

    test<fnv1a_32, default_flavor>();
    test<fnv1a_32, little_endian_flavor>();
    test<fnv1a_32, big_endian_flavor>();

    return boost::report_errors();
}

#endif
