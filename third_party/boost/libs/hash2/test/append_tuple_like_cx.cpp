// Copyright 2017, 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/hash_append.hpp>
#include <boost/hash2/fnv1a.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <utility>
#include <tuple>

#if defined(BOOST_MSVC) && BOOST_MSVC < 1920
# pragma warning(disable: 4307) // integral constant overflow
#endif

template<class Hash, class Flavor, class T>
BOOST_CXX14_CONSTEXPR typename Hash::result_type test( T const& v )
{
    Hash h;
    Flavor f{};

    hash_append( h, f, v );

    return h.result();
}

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

#if defined(BOOST_NO_CXX14_CONSTEXPR) || ( defined(BOOST_GCC) && BOOST_GCC < 60000 )

# define TEST_EQ(x1, x2) BOOST_TEST_EQ(x1, x2)

#else

# define TEST_EQ(x1, x2) BOOST_TEST_EQ(x1, x2); STATIC_ASSERT(x1 == x2)

#endif

int main()
{
    using namespace boost::hash2;

    TEST_EQ( (test<fnv1a_32, default_flavor>( std::make_pair( '\x01', '\x02' ) )), 3983810698 );
    TEST_EQ( (test<fnv1a_32, default_flavor>( std::make_tuple( '\x01', '\x02' ) )), 3983810698 );

    TEST_EQ( (test<fnv1a_32, little_endian_flavor>( std::make_pair( 1, 2 ) )), 3738734694 );
    TEST_EQ( (test<fnv1a_32, big_endian_flavor>( std::make_tuple( 1, 2 ) )), 1396319868 );

#if defined(BOOST_HASH2_HAS_BUILTIN_BIT_CAST)

    TEST_EQ( (test<fnv1a_32, little_endian_flavor>( std::make_pair( 1.0, 2.0 ) )), 1170904120 );
    TEST_EQ( (test<fnv1a_32, big_endian_flavor>( std::make_tuple( 1.0, 2.0 ) )), 786481930 );

#endif

    return boost::report_errors();
}
