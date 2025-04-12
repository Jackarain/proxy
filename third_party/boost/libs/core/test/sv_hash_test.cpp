// Copyright 2021-2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/core/detail/string_view.hpp>
#include <boost/container_hash/hash.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <string>

template<class T> std::size_t hv( T const& t )
{
    return boost::hash<T>()( t );
}

template<class Ch> void test( Ch const* p )
{
    std::basic_string<Ch> s( p );
    boost::core::basic_string_view<Ch> sv( s );

    BOOST_TEST_EQ( hv( s ), hv( sv ) );
}

int main()
{
    test( "123" );
    test( L"123" );

#if !defined(BOOST_NO_CXX11_CHAR16_T)

    test( u"123" );

#endif

#if !defined(BOOST_NO_CXX11_CHAR32_T)

    test( U"123" );

#endif

#if defined(__cpp_char8_t) && __cpp_char8_t >= 201811L

    test( u8"123" );

#endif

    return boost::report_errors();
}
