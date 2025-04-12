// Copyright 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/ripemd.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <array>

#if defined(BOOST_MSVC) && BOOST_MSVC < 1920
# pragma warning(disable: 4307) // integral constant overflow
#endif

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

#if defined(BOOST_NO_CXX14_CONSTEXPR)

# define TEST_EQ(x1, x2) BOOST_TEST_EQ(x1, x2)

#else

# define TEST_EQ(x1, x2) BOOST_TEST_EQ(x1, x2); STATIC_ASSERT(x1 == x2)

#endif

int main()
{
    using namespace boost::hash2;

    {
        BOOST_CXX14_CONSTEXPR digest<20> r = {{ 0x9C, 0x11, 0x85, 0xA5, 0xC5, 0xE9, 0xFC, 0x54, 0x61, 0x28, 0x08, 0x97, 0x7E, 0xE8, 0xF5, 0x48, 0xB2, 0x25, 0x8D, 0x31 }};

        TEST_EQ( ripemd_160().result(), r );
        TEST_EQ( ripemd_160(0).result(), r );
        TEST_EQ( ripemd_160(static_cast<unsigned char const*>(nullptr), 0).result(), r );
    }

    {
        BOOST_CXX14_CONSTEXPR digest<16> r = {{ 0xCD, 0xF2, 0x62, 0x13, 0xA1, 0x50, 0xDC, 0x3E, 0xCB, 0x61, 0x0F, 0x18, 0xF6, 0xB3, 0x8B, 0x46 }};

        TEST_EQ( ripemd_128().result(), r );
        TEST_EQ( ripemd_128(0).result(), r );
        TEST_EQ( ripemd_128(static_cast<unsigned char const*>(nullptr), 0).result(), r );
    }

    return boost::report_errors();
}
