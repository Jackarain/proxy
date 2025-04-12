// Copyright 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/hash_append.hpp>
#include <boost/hash2/fnv1a.hpp>
#include <boost/core/lightweight_test.hpp>

using namespace boost::hash2;

template<class T> void test( std::uint32_t r )
{
    {
        T v[ 95 ] = {};

        fnv1a_32 h;
        hash_append( h, {}, v );

        BOOST_TEST_EQ( h.result(), r );
    }
}

int main()
{
    std::uint32_t const r1 = 409807047;
    std::uint32_t const r2 = 4291711997;
    std::uint32_t const r4 = 3226601845;

    test<char>( r1 );

    test<char16_t>( r2 );
    test<char32_t>( r4 );

    test<wchar_t>( sizeof(wchar_t) == sizeof(std::uint16_t)? r2: r4 );

#if defined(__cpp_char8_t) && __cpp_char8_t >= 201811L
    test<char8_t>( r1 );
#endif

    return boost::report_errors();
}
