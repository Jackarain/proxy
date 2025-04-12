// Copyright 2024 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/hash_append.hpp>
#include <boost/hash2/fnv1a.hpp>
#include <boost/describe/class.hpp>
#include <boost/config.hpp>
#include <boost/config/pragma_message.hpp>

#if !defined(BOOST_DESCRIBE_CXX14)

BOOST_PRAGMA_MESSAGE( "Test skipped, because BOOST_DESCRIBE_CXX14 is not defined" )
int main() {}

#elif defined(BOOST_MSVC) && BOOST_MSVC >= 1910 && BOOST_MSVC < 1930

BOOST_PRAGMA_MESSAGE( "Test skipped, because BOOST_MSVC is 191x or 192x" )
int main() {}

#else

#include <boost/core/lightweight_test.hpp>
#include <utility>

template<class T1, class T2> struct pair
{
    T1 first;
    T2 second;

    BOOST_DESCRIBE_CLASS(pair, (), (first, second), (), ())
};

template<class Hash, class Flavor, class T1, class T2, class R> void test( pair<T1, T2> const& p1, std::pair<T1, T2> const& p2, R r )
{
    Flavor f;

    {
        Hash h;
        hash_append( h, f, p1 );

        BOOST_TEST_EQ( h.result(), r );
    }

    {
        Hash h;
        hash_append( h, f, p2 );

        BOOST_TEST_EQ( h.result(), r );
    }
}

int main()
{
    using namespace boost::hash2;

    test<fnv1a_32, default_flavor, int, float>( {}, {}, 2615243109 );
    test<fnv1a_32, little_endian_flavor, int, float>( {}, {}, 2615243109 );
    test<fnv1a_32, big_endian_flavor, int, float>( {}, {}, 2615243109 );

    test<fnv1a_32, little_endian_flavor, char, double>( { 'A', 3.14 }, { 'A', 3.14 }, 2789734447 );
    test<fnv1a_32, big_endian_flavor, char, double>( { 'A', 3.14 }, { 'A', 3.14 }, 1423350833 );

    return boost::report_errors();
}

#endif
