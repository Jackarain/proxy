// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/core/detail/string_view.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <string>

#if !defined(BOOST_NO_CXX17_HDR_STRING_VIEW)
# include <string_view>
#endif

using namespace boost::uuids;

template<class Ch> void test( uuid const& expected, Ch const* str )
{
    BOOST_TEST_EQ( string_generator()( str ), expected );
    BOOST_TEST_EQ( string_generator()( std::basic_string<Ch>( str ) ), expected );
    BOOST_TEST_EQ( string_generator()( boost::core::basic_string_view<Ch>( str ) ), expected );
#if !defined(BOOST_NO_CXX17_HDR_STRING_VIEW)
    BOOST_TEST_EQ( string_generator()( std::basic_string_view<Ch>( str ) ), expected );
#endif
}

int main()
{
    uuid const exp = {{ 0x12, 0x34, 0x56, 0x78, 0x90, 0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0x78, 0x90, 0xab, 0xcd, 0xef }};

    test( exp,   "12345678-90AB-CDEF-1234-567890abcdef" );
    test( exp,  L"12345678-90AB-CDEF-1234-567890abcdef" );
    test( exp,  u"12345678-90AB-CDEF-1234-567890abcdef" );
    test( exp,  U"12345678-90AB-CDEF-1234-567890abcdef" );
    test( exp, u8"12345678-90AB-CDEF-1234-567890abcdef" );

    test( exp,   "{12345678-90AB-CDEF-1234-567890abcdef}" );
    test( exp,  L"{12345678-90AB-CDEF-1234-567890abcdef}" );
    test( exp,  u"{12345678-90AB-CDEF-1234-567890abcdef}" );
    test( exp,  U"{12345678-90AB-CDEF-1234-567890abcdef}" );
    test( exp, u8"{12345678-90AB-CDEF-1234-567890abcdef}" );

    test( exp,   "1234567890ABCDEF1234567890abcdef" );
    test( exp,  L"1234567890ABCDEF1234567890abcdef" );
    test( exp,  u"1234567890ABCDEF1234567890abcdef" );
    test( exp,  U"1234567890ABCDEF1234567890abcdef" );
    test( exp, u8"1234567890ABCDEF1234567890abcdef" );

    test( exp,   "{1234567890ABCDEF1234567890abcdef}" );
    test( exp,  L"{1234567890ABCDEF1234567890abcdef}" );
    test( exp,  u"{1234567890ABCDEF1234567890abcdef}" );
    test( exp,  U"{1234567890ABCDEF1234567890abcdef}" );
    test( exp, u8"{1234567890ABCDEF1234567890abcdef}" );

    return boost::report_errors();
}
