// Copyright 2024 Peter Dimov.
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

enum E: unsigned char {};

int main()
{
    constexpr std::uint32_t r = 409807047;

    TEST_EQ( test<bool>(), r );
    TEST_EQ( test<char>(), r );
    TEST_EQ( test<signed char>(), r );
    TEST_EQ( test<unsigned char>(), r );
    TEST_EQ( test<E>(), r );

#if defined(__cpp_lib_byte) && __cpp_lib_byte >= 201603L
    TEST_EQ( test<std::byte>(), r );
#endif

#if defined(__cpp_char8_t) && __cpp_char8_t >= 201811L
    TEST_EQ( test<char8_t>(), r );
#endif
}
