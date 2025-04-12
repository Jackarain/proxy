// Copyright 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/md5.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <array>

#if defined(BOOST_MSVC) && BOOST_MSVC < 1920
# pragma warning(disable: 4307) // integral constant overflow
#endif

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

#if defined(BOOST_NO_CXX14_CONSTEXPR) || ( defined(BOOST_GCC) && BOOST_GCC < 60000 )

# define TEST_EQ(x1, x2) BOOST_TEST_EQ(x1, x2)

#else

# define TEST_EQ(x1, x2) BOOST_TEST_EQ(x1, x2); STATIC_ASSERT(x1 == x2)

#endif

int main()
{
    using namespace boost::hash2;

    BOOST_CXX14_CONSTEXPR digest<16> r = {{ 0x74, 0xE6, 0xF7, 0x29, 0x8A, 0x9C, 0x2D, 0x16, 0x89, 0x35, 0xF5, 0x8C, 0x00, 0x1B, 0xAD, 0x88 }};

    TEST_EQ( hmac_md5_128().result(), r );
    TEST_EQ( hmac_md5_128(0).result(), r );
    TEST_EQ( hmac_md5_128(static_cast<unsigned char const*>(nullptr), 0).result(), r );

    return boost::report_errors();
}
