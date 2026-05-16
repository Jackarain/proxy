// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#define BOOST_UUID_REPORT_IMPLEMENTATION

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/core/lightweight_test.hpp>
#include <string>

using namespace boost::uuids;

template<class Ch> void test( Ch const* str, int pos, from_chars_error err )
{
    Ch const* first = str;
    Ch const* last = first + std::char_traits<Ch>::length( first );

    uuid u;

    from_chars_result<Ch> r = from_chars( first, last, u );

    BOOST_TEST_EQ( r.ptr, first + pos );

    BOOST_TEST( r.ec == err );
    BOOST_TEST_EQ( static_cast<int>( r.ec ), static_cast<int>( err ) );
    BOOST_TEST_EQ( static_cast<bool>( r ), ( err == from_chars_error::none ) );
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

    return boost::report_errors();
}
