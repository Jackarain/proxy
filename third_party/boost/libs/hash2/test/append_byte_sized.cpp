// Copyright 2024 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/hash_append.hpp>
#include <boost/hash2/fnv1a.hpp>
#include <boost/core/lightweight_test.hpp>

using namespace boost::hash2;

template<class T> void test()
{
    {
        T v[ 95 ] = {};

        fnv1a_32 h;
        hash_append( h, {}, v );

        BOOST_TEST_EQ( h.result(), 409807047 );
    }
}

enum E: unsigned char {};

int main()
{
    test<bool>();
    test<char>();
    test<signed char>();
    test<unsigned char>();
    test<E>();

#if defined(__cpp_lib_byte) && __cpp_lib_byte >= 201603L
    test<std::byte>();
#endif

#if defined(__cpp_char8_t) && __cpp_char8_t >= 201811L
    test<char8_t>();
#endif

    return boost::report_errors();
}
