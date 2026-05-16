// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/core/lightweight_test.hpp>

#define CXX14_CONSTEXPR BOOST_CXX14_CONSTEXPR

#if defined(BOOST_GCC) && BOOST_GCC < 60000

// GCC 5 doesn't consider string_generator::operator()(first, last) constexpr

# undef CXX14_CONSTEXPR
# define CXX14_CONSTEXPR

#endif

using namespace boost::uuids;

#define TEST(str) { CXX14_CONSTEXPR auto u = string_generator()(str); BOOST_TEST_EQ(u, expected); }

int main()
{
    uuid const expected = {{ 0x12, 0x34, 0x56, 0x78, 0x90, 0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0x78, 0x90, 0xab, 0xcd, 0xef }};

    TEST(   "12345678-90AB-CDEF-1234-567890abcdef" );
    TEST(  L"12345678-90AB-CDEF-1234-567890abcdef" );
    TEST(  u"12345678-90AB-CDEF-1234-567890abcdef" );
    TEST(  U"12345678-90AB-CDEF-1234-567890abcdef" );
    TEST( u8"12345678-90AB-CDEF-1234-567890abcdef" );

    TEST(   "{12345678-90AB-CDEF-1234-567890abcdef}" );
    TEST(  L"{12345678-90AB-CDEF-1234-567890abcdef}" );
    TEST(  u"{12345678-90AB-CDEF-1234-567890abcdef}" );
    TEST(  U"{12345678-90AB-CDEF-1234-567890abcdef}" );
    TEST( u8"{12345678-90AB-CDEF-1234-567890abcdef}" );

    TEST(   "1234567890ABCDEF1234567890abcdef" );
    TEST(  L"1234567890ABCDEF1234567890abcdef" );
    TEST(  u"1234567890ABCDEF1234567890abcdef" );
    TEST(  U"1234567890ABCDEF1234567890abcdef" );
    TEST( u8"1234567890ABCDEF1234567890abcdef" );

    TEST(   "{1234567890ABCDEF1234567890abcdef}" );
    TEST(  L"{1234567890ABCDEF1234567890abcdef}" );
    TEST(  u"{1234567890ABCDEF1234567890abcdef}" );
    TEST(  U"{1234567890ABCDEF1234567890abcdef}" );
    TEST( u8"{1234567890ABCDEF1234567890abcdef}" );

    return boost::report_errors();
}
