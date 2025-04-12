// Copyright 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/hash_append.hpp>
#include <boost/hash2/fnv1a.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <cstdint>

#if defined(BOOST_MSVC) && BOOST_MSVC < 1920
# pragma warning(disable: 4307) // integral constant overflow
#endif

using namespace boost::hash2;

template<class T> BOOST_CXX14_CONSTEXPR std::uint32_t test()
{
    T v[ 95 ] = {};

    fnv1a_32 h;
    hash_append( h, {}, v );

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
    std::uint32_t const r1 = 409807047;
    std::uint32_t const r2 = 4291711997;
    std::uint32_t const r4 = 3226601845;

    TEST_EQ( test<char>(), r1 );

    TEST_EQ( test<char16_t>(), r2 );
    TEST_EQ( test<char32_t>(), r4 );

    TEST_EQ( test<wchar_t>(), (sizeof(wchar_t) == sizeof(std::uint16_t)? r2: r4) );

#if defined(__cpp_char8_t) && __cpp_char8_t >= 201811L
    TEST_EQ( test<char8_t>(), r1 );
#endif

    return boost::report_errors();
}
