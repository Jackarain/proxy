// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <cstddef>

using namespace boost::uuids;

struct test_result
{
    std::ptrdiff_t pos;
    from_chars_error ec;
};

template<class Ch, std::size_t N>
BOOST_UUID_CXX14_CONSTEXPR_RT test_result test( Ch const (&str)[ N ] )
{
    Ch const* first = str;
    Ch const* last = first + N - 1;

    uuid u;
    from_chars_result<Ch> r = from_chars( first, last, u );

    return { r.ptr - first, r.ec };
}

#define TEST(Str, Pos, Ec) { BOOST_UUID_CXX14_CONSTEXPR_RT auto r = test(Str); BOOST_TEST_EQ(Pos, r.pos); BOOST_TEST_EQ(static_cast<int>(Ec), static_cast<int>(r.ec)); }

int main()
{
    TEST(   "", 0, from_chars_error::unexpected_end_of_input );
    TEST(  L"", 0, from_chars_error::unexpected_end_of_input );
    TEST(  u"", 0, from_chars_error::unexpected_end_of_input );
    TEST(  U"", 0, from_chars_error::unexpected_end_of_input );
    TEST( u8"", 0, from_chars_error::unexpected_end_of_input );

    TEST(   "@", 0, from_chars_error::hex_digit_expected );
    TEST(  L"@", 0, from_chars_error::hex_digit_expected );
    TEST(  u"@", 0, from_chars_error::hex_digit_expected );
    TEST(  U"@", 0, from_chars_error::hex_digit_expected );
    TEST( u8"@", 0, from_chars_error::hex_digit_expected );

    TEST(   "G", 0, from_chars_error::hex_digit_expected );
    TEST(  L"G", 0, from_chars_error::hex_digit_expected );
    TEST(  u"G", 0, from_chars_error::hex_digit_expected );
    TEST(  U"G", 0, from_chars_error::hex_digit_expected );
    TEST( u8"G", 0, from_chars_error::hex_digit_expected );

    TEST(   "g", 0, from_chars_error::hex_digit_expected );
    TEST(  L"g", 0, from_chars_error::hex_digit_expected );
    TEST(  u"g", 0, from_chars_error::hex_digit_expected );
    TEST(  U"g", 0, from_chars_error::hex_digit_expected );
    TEST( u8"g", 0, from_chars_error::hex_digit_expected );

    TEST(   "-", 0, from_chars_error::hex_digit_expected );
    TEST(  L"-", 0, from_chars_error::hex_digit_expected );
    TEST(  u"-", 0, from_chars_error::hex_digit_expected );
    TEST(  U"-", 0, from_chars_error::hex_digit_expected );
    TEST( u8"-", 0, from_chars_error::hex_digit_expected );

    TEST(   "abcdefgh-0000-0000-0000-000000000000", 6, from_chars_error::hex_digit_expected );
    TEST(  L"abcdefgh-0000-0000-0000-000000000000", 6, from_chars_error::hex_digit_expected );
    TEST(  u"abcdefgh-0000-0000-0000-000000000000", 6, from_chars_error::hex_digit_expected );
    TEST(  U"abcdefgh-0000-0000-0000-000000000000", 6, from_chars_error::hex_digit_expected );
    TEST( u8"abcdefgh-0000-0000-0000-000000000000", 6, from_chars_error::hex_digit_expected );

    TEST(   "Have a nice day", 0, from_chars_error::hex_digit_expected );
    TEST(  L"Have a nice day", 0, from_chars_error::hex_digit_expected );
    TEST(  u"Have a nice day", 0, from_chars_error::hex_digit_expected );
    TEST(  U"Have a nice day", 0, from_chars_error::hex_digit_expected );
    TEST( u8"Have a nice day", 0, from_chars_error::hex_digit_expected );

    TEST(   "0000000000000-0000-0000-000000000000", 8, from_chars_error::dash_expected );
    TEST(  L"0000000000000-0000-0000-000000000000", 8, from_chars_error::dash_expected );
    TEST(  u"0000000000000-0000-0000-000000000000", 8, from_chars_error::dash_expected );
    TEST(  U"0000000000000-0000-0000-000000000000", 8, from_chars_error::dash_expected );
    TEST( u8"0000000000000-0000-0000-000000000000", 8, from_chars_error::dash_expected );

    TEST(   "00000000-000000000-0000-000000000000", 13, from_chars_error::dash_expected );
    TEST(  L"00000000-000000000-0000-000000000000", 13, from_chars_error::dash_expected );
    TEST(  u"00000000-000000000-0000-000000000000", 13, from_chars_error::dash_expected );
    TEST(  U"00000000-000000000-0000-000000000000", 13, from_chars_error::dash_expected );
    TEST( u8"00000000-000000000-0000-000000000000", 13, from_chars_error::dash_expected );

    TEST(   "00000000-0000-000000000-000000000000", 18, from_chars_error::dash_expected );
    TEST(  L"00000000-0000-000000000-000000000000", 18, from_chars_error::dash_expected );
    TEST(  u"00000000-0000-000000000-000000000000", 18, from_chars_error::dash_expected );
    TEST(  U"00000000-0000-000000000-000000000000", 18, from_chars_error::dash_expected );
    TEST( u8"00000000-0000-000000000-000000000000", 18, from_chars_error::dash_expected );

    TEST(   "00000000-0000-0000-00000000000000000", 23, from_chars_error::dash_expected );
    TEST(  L"00000000-0000-0000-00000000000000000", 23, from_chars_error::dash_expected );
    TEST(  u"00000000-0000-0000-00000000000000000", 23, from_chars_error::dash_expected );
    TEST(  U"00000000-0000-0000-00000000000000000", 23, from_chars_error::dash_expected );
    TEST( u8"00000000-0000-0000-00000000000000000", 23, from_chars_error::dash_expected );

    return boost::report_errors();
}
