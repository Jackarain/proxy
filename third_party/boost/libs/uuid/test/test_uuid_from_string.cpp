// Copyright 2025, 2026 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/core/detail/string_view.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <string>

#if !defined(BOOST_NO_CXX17_HDR_STRING_VIEW)
# include <string_view>
#endif

//

using namespace boost::uuids;

template<class Ch> void test( uuid const& expected, Ch const* str )
{
    BOOST_TEST_EQ( uuid_from_string( str ), expected );
    BOOST_TEST_EQ( uuid_from_string( std::basic_string<Ch>( str ) ), expected );
    BOOST_TEST_EQ( uuid_from_string( boost::core::basic_string_view<Ch>( str ) ), expected );
#if !defined(BOOST_NO_CXX17_HDR_STRING_VIEW)
    BOOST_TEST_EQ( uuid_from_string( std::basic_string_view<Ch>( str ) ), expected );
#endif
}

int main()
{
    uuid const u1 = {{ 0 }};

    test( u1,   "00000000-0000-0000-0000-000000000000" );
    test( u1,  L"00000000-0000-0000-0000-000000000000" );
    test( u1,  u"00000000-0000-0000-0000-000000000000" );
    test( u1,  U"00000000-0000-0000-0000-000000000000" );
    test( u1, u8"00000000-0000-0000-0000-000000000000" );

    uuid const u2 = {{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f }};

    test( u2,   "00010203-0405-0607-0809-0a0b0c0d0e0f" );
    test( u2,  L"00010203-0405-0607-0809-0a0b0c0d0e0f" );
    test( u2,  u"00010203-0405-0607-0809-0a0b0c0d0e0f" );
    test( u2,  U"00010203-0405-0607-0809-0a0b0c0d0e0f" );
    test( u2, u8"00010203-0405-0607-0809-0a0b0c0d0e0f" );

    test( u2,   "00010203-0405-0607-0809-0A0B0C0D0E0F" );
    test( u2,  L"00010203-0405-0607-0809-0A0B0C0D0E0F" );
    test( u2,  u"00010203-0405-0607-0809-0A0B0C0D0E0F" );
    test( u2,  U"00010203-0405-0607-0809-0A0B0C0D0E0F" );
    test( u2, u8"00010203-0405-0607-0809-0A0B0C0D0E0F" );

    uuid const u3 = {{ 0x12, 0x34, 0x56, 0x78, 0x90, 0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0x78, 0x90, 0xab, 0xcd, 0xef }};

    test( u3,   "12345678-90ab-cdef-1234-567890abcdef" );
    test( u3,  L"12345678-90ab-cdef-1234-567890abcdef" );
    test( u3,  u"12345678-90ab-cdef-1234-567890abcdef" );
    test( u3,  U"12345678-90ab-cdef-1234-567890abcdef" );
    test( u3, u8"12345678-90ab-cdef-1234-567890abcdef" );

    test( u3,   "12345678-90AB-CDEF-1234-567890abcdef" );
    test( u3,  L"12345678-90AB-CDEF-1234-567890abcdef" );
    test( u3,  u"12345678-90AB-CDEF-1234-567890abcdef" );
    test( u3,  U"12345678-90AB-CDEF-1234-567890abcdef" );
    test( u3, u8"12345678-90AB-CDEF-1234-567890abcdef" );

    return boost::report_errors();
}
