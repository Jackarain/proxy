// Copyright 2017, 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/hash_append.hpp>
#include <boost/hash2/fnv1a.hpp>
#include <boost/core/lightweight_test.hpp>
#include <string>
#include <vector>
#include <list>

template<class Hash, class Flavor, class T, class R> void test_container( R r )
{
    unsigned char w[] = { 1, 2, 3, 4 };
    T v( w, w + sizeof(w) / sizeof(w[0]) );

    Flavor f;

    {
        Hash h;
        boost::hash2::hash_append( h, f, v );

        BOOST_TEST_EQ( h.result(), r );
    }

    {
        Hash h;
        boost::hash2::hash_append_range_and_size( h, f, v.begin(), v.end() );

        BOOST_TEST_EQ( h.result(), r );
    }
}

template<class Hash, class Flavor, class T, class R> void test( R r )
{
    test_container< Hash, Flavor, std::basic_string<T> >( r );
    test_container< Hash, Flavor, std::vector<T> >( r );
    test_container< Hash, Flavor, std::list<T> >( r );
}

int main()
{
    using namespace boost::hash2;

    test<fnv1a_32, little_endian_flavor, char>( 2227238665 );
    test<fnv1a_32, big_endian_flavor, char>( 3245468929 );

#if defined(__cpp_char8_t) && __cpp_char8_t >= 201811L

    test<fnv1a_32, little_endian_flavor, char8_t>( 2227238665 );
    test<fnv1a_32, big_endian_flavor, char8_t>( 3245468929 );

#endif

    test<fnv1a_32, little_endian_flavor, char16_t>( 2981571797 );
    test<fnv1a_32, big_endian_flavor, char16_t>( 1063054677 );

    test<fnv1a_32, little_endian_flavor, char32_t>( 1745310485 );
    test<fnv1a_32, big_endian_flavor, char32_t>( 3959736277 );

    return boost::report_errors();
}
