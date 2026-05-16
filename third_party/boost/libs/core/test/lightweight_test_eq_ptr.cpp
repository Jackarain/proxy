// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>

int main()
{
    {
        char const* p = "12";

        BOOST_TEST_EQ( p, p );
        BOOST_TEST_NE( p, p + 1 );
    }

    {
        wchar_t const* p = L"12";

        BOOST_TEST_EQ( p, p );
        BOOST_TEST_NE( p, p + 1 );
    }

#if !defined( BOOST_NO_CXX11_CHAR16_T )

    {
        char16_t const* p = u"12";

        BOOST_TEST_EQ( p, p );
        BOOST_TEST_NE( p, p + 1 );
    }

#endif

#if !defined( BOOST_NO_CXX11_CHAR32_T )

    {
        char32_t const* p = U"12";

        BOOST_TEST_EQ( p, p );
        BOOST_TEST_NE( p, p + 1 );
    }

#endif

#if defined(__cpp_char8_t) && __cpp_char8_t >= 201811L

    {
        char8_t const* p = u8"12";

        BOOST_TEST_EQ( p, p );
        BOOST_TEST_NE( p, p + 1 );
    }

#endif

    {
        int v;
        int volatile* p = &v;

        BOOST_TEST_EQ( p, p );
        BOOST_TEST_NE( p, p + 1 );
    }

    {
        int v;
        int const volatile* p = &v;

        BOOST_TEST_EQ( p, p );
        BOOST_TEST_NE( p, p + 1 );
    }

    {
        char v;
        char volatile* p = &v;

        BOOST_TEST_EQ( p, p );
        BOOST_TEST_NE( p, p + 1 );
    }

    {
        char v;
        char const volatile* p = &v;

        BOOST_TEST_EQ( p, p );
        BOOST_TEST_NE( p, p + 1 );
    }

    {
        wchar_t v;
        wchar_t volatile* p = &v;

        BOOST_TEST_EQ( p, p );
        BOOST_TEST_NE( p, p + 1 );
    }

    {
        wchar_t v;
        wchar_t const volatile* p = &v;

        BOOST_TEST_EQ( p, p );
        BOOST_TEST_NE( p, p + 1 );
    }

    return boost::report_errors();
}
