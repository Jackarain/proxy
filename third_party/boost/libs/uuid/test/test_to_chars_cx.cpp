// Copyright 2024, 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config/pragma_message.hpp>
#include <boost/array.hpp>
#include <boost/config.hpp>
#include <string>

#if defined(BOOST_UUID_NO_CXX14_CONSTEXPR_RT)
BOOST_PRAGMA_MESSAGE( "Test is not constexpr because BOOST_UUID_NO_CXX14_CONSTEXPR_RT is defined" )
#endif

using namespace boost::uuids;

template<class Ch>
BOOST_UUID_CXX14_CONSTEXPR_RT boost::array<Ch, 36> uuid_to_string( uuid const& u )
{
    boost::array<Ch, 36> r = {{}};
    to_chars( u, r.begin(), r.end() );
    return r;
}

int main()
{
    {
        BOOST_CXX14_CONSTEXPR uuid u;

        {
            BOOST_UUID_CXX14_CONSTEXPR_RT auto v = uuid_to_string<char>( u );

            std::string const w( "00000000-0000-0000-0000-000000000000" );

            BOOST_TEST_ALL_EQ( v.begin(), v.end(), w.begin(), w.end() );
        }

        {
            BOOST_UUID_CXX14_CONSTEXPR_RT auto v = uuid_to_string<wchar_t>( u );

            std::wstring const w( L"00000000-0000-0000-0000-000000000000" );

            BOOST_TEST_ALL_EQ( v.begin(), v.end(), w.begin(), w.end() );
        }

        {
            BOOST_UUID_CXX14_CONSTEXPR_RT auto v = uuid_to_string<char16_t>( u );

            std::u16string const w( u"00000000-0000-0000-0000-000000000000" );

            BOOST_TEST_ALL_EQ( v.begin(), v.end(), w.begin(), w.end() );
        }

        {
            BOOST_UUID_CXX14_CONSTEXPR_RT auto v = uuid_to_string<char32_t>( u );

            std::u32string const w( U"00000000-0000-0000-0000-000000000000" );

            BOOST_TEST_ALL_EQ( v.begin(), v.end(), w.begin(), w.end() );
        }

#if defined(__cpp_char8_t) && __cpp_char8_t >= 201811L

        {
            BOOST_UUID_CXX14_CONSTEXPR_RT auto v = uuid_to_string<char8_t>( u );

            std::u8string const w( u8"00000000-0000-0000-0000-000000000000" );

            BOOST_TEST_ALL_EQ( v.begin(), v.end(), w.begin(), w.end() );
        }

#endif
    }

    {
        BOOST_CXX14_CONSTEXPR uuid u = {{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f }};

        {
            BOOST_UUID_CXX14_CONSTEXPR_RT auto v = uuid_to_string<char>( u );

            std::string const w( "00010203-0405-0607-0809-0a0b0c0d0e0f" );

            BOOST_TEST_ALL_EQ( v.begin(), v.end(), w.begin(), w.end() );
        }

        {
            BOOST_UUID_CXX14_CONSTEXPR_RT auto v = uuid_to_string<wchar_t>( u );

            std::wstring const w( L"00010203-0405-0607-0809-0a0b0c0d0e0f" );

            BOOST_TEST_ALL_EQ( v.begin(), v.end(), w.begin(), w.end() );
        }

        {
            BOOST_UUID_CXX14_CONSTEXPR_RT auto v = uuid_to_string<char16_t>( u );

            std::u16string const w( u"00010203-0405-0607-0809-0a0b0c0d0e0f" );

            BOOST_TEST_ALL_EQ( v.begin(), v.end(), w.begin(), w.end() );
        }

        {
            BOOST_UUID_CXX14_CONSTEXPR_RT auto v = uuid_to_string<char32_t>( u );

            std::u32string const w( U"00010203-0405-0607-0809-0a0b0c0d0e0f" );

            BOOST_TEST_ALL_EQ( v.begin(), v.end(), w.begin(), w.end() );
        }

#if defined(__cpp_char8_t) && __cpp_char8_t >= 201811L

        {
            BOOST_UUID_CXX14_CONSTEXPR_RT auto v = uuid_to_string<char8_t>( u );

            std::u8string const w( u8"00010203-0405-0607-0809-0a0b0c0d0e0f" );

            BOOST_TEST_ALL_EQ( v.begin(), v.end(), w.begin(), w.end() );
        }

#endif
    }

    {
        BOOST_CXX14_CONSTEXPR uuid u = {{ 0x12, 0x34, 0x56, 0x78, 0x90, 0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0x78, 0x90, 0xab, 0xcd, 0xef }};

        {
            BOOST_UUID_CXX14_CONSTEXPR_RT auto v = uuid_to_string<char>( u );

            std::string const w( "12345678-90ab-cdef-1234-567890abcdef" );

            BOOST_TEST_ALL_EQ( v.begin(), v.end(), w.begin(), w.end() );
        }

        {
            BOOST_UUID_CXX14_CONSTEXPR_RT auto v = uuid_to_string<wchar_t>( u );

            std::wstring const w( L"12345678-90ab-cdef-1234-567890abcdef" );

            BOOST_TEST_ALL_EQ( v.begin(), v.end(), w.begin(), w.end() );
        }

        {
            BOOST_UUID_CXX14_CONSTEXPR_RT auto v = uuid_to_string<char16_t>( u );

            std::u16string const w( u"12345678-90ab-cdef-1234-567890abcdef" );

            BOOST_TEST_ALL_EQ( v.begin(), v.end(), w.begin(), w.end() );
        }

        {
            BOOST_UUID_CXX14_CONSTEXPR_RT auto v = uuid_to_string<char32_t>( u );

            std::u32string const w( U"12345678-90ab-cdef-1234-567890abcdef" );

            BOOST_TEST_ALL_EQ( v.begin(), v.end(), w.begin(), w.end() );
        }

#if defined(__cpp_char8_t) && __cpp_char8_t >= 201811L

        {
            BOOST_UUID_CXX14_CONSTEXPR_RT auto v = uuid_to_string<char8_t>( u );

            std::u8string const w( u8"12345678-90ab-cdef-1234-567890abcdef" );

            BOOST_TEST_ALL_EQ( v.begin(), v.end(), w.begin(), w.end() );
        }

#endif
    }

    return boost::report_errors();
}
