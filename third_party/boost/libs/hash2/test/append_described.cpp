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
    int a;
    int b;
};

BOOST_DESCRIBE_STRUCT(X, (), (a, b))

template<class Hash, class Flavor, class R> void test( X const& x, int const (&a)[ 2 ], R r )
{
    Flavor f;

    {
        Hash h;
        hash_append( h, f, x );

        BOOST_TEST_EQ( h.result(), r );
    }

    {
        Hash h;
        hash_append( h, f, a );

        BOOST_TEST_EQ( h.result(), r );
    }
}

int main()
{
    using namespace boost::hash2;

    test<fnv1a_32, default_flavor>( {}, {}, 2615243109 );
    test<fnv1a_32, little_endian_flavor>( {}, {}, 2615243109 );
    test<fnv1a_32, big_endian_flavor>( {}, {}, 2615243109 );

    test<fnv1a_32, little_endian_flavor>( { 1, 2 }, { 1, 2 }, 3738734694 );
    test<fnv1a_32, big_endian_flavor>( { 1, 2 }, { 1, 2 }, 1396319868 );

    return boost::report_errors();
}

#endif
