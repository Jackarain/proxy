// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <cstddef>

using namespace boost::uuids;

template<class Ch, std::size_t N>
BOOST_UUID_CXX14_CONSTEXPR_RT uuid uuid_from_string( Ch const (&str)[ N ] )
{
    Ch const* first = str;
    Ch const* last = first + N - 1;

    uuid u;
    from_chars( first, last, u );

    return u;
}

#define TEST(str) { BOOST_UUID_CXX14_CONSTEXPR_RT auto u = ::uuid_from_string(str); BOOST_TEST_EQ(u, expected); }

int main()
{
    {
        uuid const expected = {{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f }};

        TEST(   "00010203-0405-0607-0809-0a0b0c0d0e0f" );
        TEST(  L"00010203-0405-0607-0809-0a0b0c0d0e0f" );
        TEST(  u"00010203-0405-0607-0809-0a0b0c0d0e0f" );
        TEST(  U"00010203-0405-0607-0809-0a0b0c0d0e0f" );
        TEST( u8"00010203-0405-0607-0809-0a0b0c0d0e0f" );

        TEST(   "00010203-0405-0607-0809-0a0b0c0d0e0f" );
        TEST(  L"00010203-0405-0607-0809-0a0b0c0d0e0f" );
        TEST(  u"00010203-0405-0607-0809-0a0b0c0d0e0f" );
        TEST(  U"00010203-0405-0607-0809-0a0b0c0d0e0f" );
        TEST( u8"00010203-0405-0607-0809-0a0b0c0d0e0f" );

        TEST(   "00010203-0405-0607-0809-0a0b0c0d0e0f1011" );
        TEST(  L"00010203-0405-0607-0809-0a0b0c0d0e0f1011" );
        TEST(  u"00010203-0405-0607-0809-0a0b0c0d0e0f1011" );
        TEST(  U"00010203-0405-0607-0809-0a0b0c0d0e0f1011" );
        TEST( u8"00010203-0405-0607-0809-0a0b0c0d0e0f1011" );

        TEST(   "00010203-0405-0607-0809-0A0B0C0D0E0F" );
        TEST(  L"00010203-0405-0607-0809-0A0B0C0D0E0F" );
        TEST(  u"00010203-0405-0607-0809-0A0B0C0D0E0F" );
        TEST(  U"00010203-0405-0607-0809-0A0B0C0D0E0F" );
        TEST( u8"00010203-0405-0607-0809-0A0B0C0D0E0F" );
    }

    {
        uuid const expected = {{ 0x12, 0x34, 0x56, 0x78, 0x90, 0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0x78, 0x90, 0xab, 0xcd, 0xef }};

        TEST(   "12345678-90ab-cdef-1234-567890abcdef" );
        TEST(  L"12345678-90ab-cdef-1234-567890abcdef" );
        TEST(  u"12345678-90ab-cdef-1234-567890abcdef" );
        TEST(  U"12345678-90ab-cdef-1234-567890abcdef" );
        TEST( u8"12345678-90ab-cdef-1234-567890abcdef" );

        TEST(   "12345678-90AB-CDEF-1234-567890abcdef" );
        TEST(  L"12345678-90AB-CDEF-1234-567890abcdef" );
        TEST(  u"12345678-90AB-CDEF-1234-567890abcdef" );
        TEST(  U"12345678-90AB-CDEF-1234-567890abcdef" );
        TEST( u8"12345678-90AB-CDEF-1234-567890abcdef" );

        TEST(   "12345678-90AB-CDEF-1234-567890abcdefghij" );
        TEST(  L"12345678-90AB-CDEF-1234-567890abcdefghij" );
        TEST(  u"12345678-90AB-CDEF-1234-567890abcdefghij" );
        TEST(  U"12345678-90AB-CDEF-1234-567890abcdefghij" );
        TEST( u8"12345678-90AB-CDEF-1234-567890abcdefghij" );
    }

    return boost::report_errors();
}
