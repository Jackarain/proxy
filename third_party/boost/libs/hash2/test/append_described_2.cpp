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
    int a = 1;
    int b = 2;
};

BOOST_DESCRIBE_STRUCT(X, (), (a, b))

struct Y
{
    int c = 3;
    int d = 4;
};

BOOST_DESCRIBE_STRUCT(Y, (), (c, d))

struct Z: X, Y
{
    int e = 5;
    int f = 6;
};

BOOST_DESCRIBE_STRUCT(Z, (X, Y), (e, f))

template<class Hash, class Flavor, class R> void test( Z const& z, int const (&a)[ 6 ], R r )
{
    Flavor f;

    {
        Hash h;
        hash_append( h, f, z );

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

    test<fnv1a_32, little_endian_flavor>( {}, { 1, 2, 3, 4, 5, 6 }, 2412827170 );
    test<fnv1a_32, big_endian_flavor>( {}, { 1, 2, 3, 4, 5, 6 }, 1970495216 );

    return boost::report_errors();
}

#endif
