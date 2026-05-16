// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/invalid_uuid.hpp>
#include <boost/core/lightweight_test.hpp>
#include <string>

using namespace boost::uuids;

template<class Ch> void test( Ch const* str, int pos, from_chars_error err )
{
    try
    {
        uuid_from_string( str );
        BOOST_ERROR( "uuid_from_string failed to throw" );
    }
    catch( invalid_uuid const& x )
    {
        BOOST_TEST_EQ( x.position(), pos );
        BOOST_TEST_EQ( static_cast<int>( x.error() ), static_cast<int>( err ) );
    }
    catch( std::exception const& /*x*/ )
    {
        BOOST_ERROR( "uuid_from_string failed to throw invalid_uuid" );
    }
}

int main()
{
    test(   "", 0, from_chars_error::unexpected_end_of_input );
    test(  L"", 0, from_chars_error::unexpected_end_of_input );
    test(  u"", 0, from_chars_error::unexpected_end_of_input );
    test(  U"", 0, from_chars_error::unexpected_end_of_input );
    test( u8"", 0, from_chars_error::unexpected_end_of_input );

    test(   "0", 1, from_chars_error::unexpected_end_of_input );
    test(  L"0", 1, from_chars_error::unexpected_end_of_input );
    test(  u"0", 1, from_chars_error::unexpected_end_of_input );
    test(  U"0", 1, from_chars_error::unexpected_end_of_input );
    test( u8"0", 1, from_chars_error::unexpected_end_of_input );

    test(   "01", 2, from_chars_error::unexpected_end_of_input );
    test(  L"01", 2, from_chars_error::unexpected_end_of_input );
    test(  u"01", 2, from_chars_error::unexpected_end_of_input );
    test(  U"01", 2, from_chars_error::unexpected_end_of_input );
    test( u8"01", 2, from_chars_error::unexpected_end_of_input );

    test(   "01234567-89aB-cDeF-0123-456789AbCd", 34, from_chars_error::unexpected_end_of_input );
    test(  L"01234567-89aB-cDeF-0123-456789AbCd", 34, from_chars_error::unexpected_end_of_input );
    test(  u"01234567-89aB-cDeF-0123-456789AbCd", 34, from_chars_error::unexpected_end_of_input );
    test(  U"01234567-89aB-cDeF-0123-456789AbCd", 34, from_chars_error::unexpected_end_of_input );
    test( u8"01234567-89aB-cDeF-0123-456789AbCd", 34, from_chars_error::unexpected_end_of_input );

    test(   "01234567-89aB-cDeF-0123-456789AbCdE", 35, from_chars_error::unexpected_end_of_input );
    test(  L"01234567-89aB-cDeF-0123-456789AbCdE", 35, from_chars_error::unexpected_end_of_input );
    test(  u"01234567-89aB-cDeF-0123-456789AbCdE", 35, from_chars_error::unexpected_end_of_input );
    test(  U"01234567-89aB-cDeF-0123-456789AbCdE", 35, from_chars_error::unexpected_end_of_input );
    test( u8"01234567-89aB-cDeF-0123-456789AbCdE", 35, from_chars_error::unexpected_end_of_input );

    test(   "@", 0, from_chars_error::hex_digit_expected );
    test(  L"@", 0, from_chars_error::hex_digit_expected );
    test(  u"@", 0, from_chars_error::hex_digit_expected );
    test(  U"@", 0, from_chars_error::hex_digit_expected );
    test( u8"@", 0, from_chars_error::hex_digit_expected );

    test(   "G", 0, from_chars_error::hex_digit_expected );
    test(  L"G", 0, from_chars_error::hex_digit_expected );
    test(  u"G", 0, from_chars_error::hex_digit_expected );
    test(  U"G", 0, from_chars_error::hex_digit_expected );
    test( u8"G", 0, from_chars_error::hex_digit_expected );

    test(   "g", 0, from_chars_error::hex_digit_expected );
    test(  L"g", 0, from_chars_error::hex_digit_expected );
    test(  u"g", 0, from_chars_error::hex_digit_expected );
    test(  U"g", 0, from_chars_error::hex_digit_expected );
    test( u8"g", 0, from_chars_error::hex_digit_expected );

    test(   "-", 0, from_chars_error::hex_digit_expected );
    test(  L"-", 0, from_chars_error::hex_digit_expected );
    test(  u"-", 0, from_chars_error::hex_digit_expected );
    test(  U"-", 0, from_chars_error::hex_digit_expected );
    test( u8"-", 0, from_chars_error::hex_digit_expected );

    test(   "abcdefgh-0000-0000-0000-000000000000", 6, from_chars_error::hex_digit_expected );
    test(  L"abcdefgh-0000-0000-0000-000000000000", 6, from_chars_error::hex_digit_expected );
    test(  u"abcdefgh-0000-0000-0000-000000000000", 6, from_chars_error::hex_digit_expected );
    test(  U"abcdefgh-0000-0000-0000-000000000000", 6, from_chars_error::hex_digit_expected );
    test( u8"abcdefgh-0000-0000-0000-000000000000", 6, from_chars_error::hex_digit_expected );

    test(   "Have a nice day", 0, from_chars_error::hex_digit_expected );
    test(  L"Have a nice day", 0, from_chars_error::hex_digit_expected );
    test(  u"Have a nice day", 0, from_chars_error::hex_digit_expected );
    test(  U"Have a nice day", 0, from_chars_error::hex_digit_expected );
    test( u8"Have a nice day", 0, from_chars_error::hex_digit_expected );

    test(   "0000000000000-0000-0000-000000000000", 8, from_chars_error::dash_expected );
    test(  L"0000000000000-0000-0000-000000000000", 8, from_chars_error::dash_expected );
    test(  u"0000000000000-0000-0000-000000000000", 8, from_chars_error::dash_expected );
    test(  U"0000000000000-0000-0000-000000000000", 8, from_chars_error::dash_expected );
    test( u8"0000000000000-0000-0000-000000000000", 8, from_chars_error::dash_expected );

    test(   "00000000-000000000-0000-000000000000", 13, from_chars_error::dash_expected );
    test(  L"00000000-000000000-0000-000000000000", 13, from_chars_error::dash_expected );
    test(  u"00000000-000000000-0000-000000000000", 13, from_chars_error::dash_expected );
    test(  U"00000000-000000000-0000-000000000000", 13, from_chars_error::dash_expected );
    test( u8"00000000-000000000-0000-000000000000", 13, from_chars_error::dash_expected );

    test(   "00000000-0000-000000000-000000000000", 18, from_chars_error::dash_expected );
    test(  L"00000000-0000-000000000-000000000000", 18, from_chars_error::dash_expected );
    test(  u"00000000-0000-000000000-000000000000", 18, from_chars_error::dash_expected );
    test(  U"00000000-0000-000000000-000000000000", 18, from_chars_error::dash_expected );
    test( u8"00000000-0000-000000000-000000000000", 18, from_chars_error::dash_expected );

    test(   "00000000-0000-0000-00000000000000000", 23, from_chars_error::dash_expected );
    test(  L"00000000-0000-0000-00000000000000000", 23, from_chars_error::dash_expected );
    test(  u"00000000-0000-0000-00000000000000000", 23, from_chars_error::dash_expected );
    test(  U"00000000-0000-0000-00000000000000000", 23, from_chars_error::dash_expected );
    test( u8"00000000-0000-0000-00000000000000000", 23, from_chars_error::dash_expected );

    test(   "00000000-0000-0000-0000-000000000000-0000", 36, from_chars_error::unexpected_extra_input );
    test(  L"00000000-0000-0000-0000-000000000000-0000", 36, from_chars_error::unexpected_extra_input );
    test(  u"00000000-0000-0000-0000-000000000000-0000", 36, from_chars_error::unexpected_extra_input );
    test(  U"00000000-0000-0000-0000-000000000000-0000", 36, from_chars_error::unexpected_extra_input );
    test( u8"00000000-0000-0000-0000-000000000000-0000", 36, from_chars_error::unexpected_extra_input );

    test(   "00010203-0405-0607-0809-0a0b0c0d0e0f1011", 36, from_chars_error::unexpected_extra_input );
    test(  L"00010203-0405-0607-0809-0a0b0c0d0e0f1011", 36, from_chars_error::unexpected_extra_input );
    test(  u"00010203-0405-0607-0809-0a0b0c0d0e0f1011", 36, from_chars_error::unexpected_extra_input );
    test(  U"00010203-0405-0607-0809-0a0b0c0d0e0f1011", 36, from_chars_error::unexpected_extra_input );
    test( u8"00010203-0405-0607-0809-0a0b0c0d0e0f1011", 36, from_chars_error::unexpected_extra_input );

    test(   "12345678-90AB-CDEF-1234-567890abcdefghij", 36, from_chars_error::unexpected_extra_input );
    test(  L"12345678-90AB-CDEF-1234-567890abcdefghij", 36, from_chars_error::unexpected_extra_input );
    test(  u"12345678-90AB-CDEF-1234-567890abcdefghij", 36, from_chars_error::unexpected_extra_input );
    test(  U"12345678-90AB-CDEF-1234-567890abcdefghij", 36, from_chars_error::unexpected_extra_input );
    test( u8"12345678-90AB-CDEF-1234-567890abcdefghij", 36, from_chars_error::unexpected_extra_input );

    return boost::report_errors();
}
