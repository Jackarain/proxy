// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>

#define CXX14_CONSTEXPR BOOST_CXX14_CONSTEXPR

#if defined(BOOST_GCC) && BOOST_GCC < 60000

// GCC 5 doesn't consider string_generator::operator()(first, last) constexpr

# undef CXX14_CONSTEXPR
# define CXX14_CONSTEXPR

#endif

CXX14_CONSTEXPR boost::uuids::string_generator gen;

CXX14_CONSTEXPR auto u1 = gen( "00000000-0000-0000-0000-000000000000" );
CXX14_CONSTEXPR auto u2 = gen( "0123456789abcdef0123456789ABCDEF" );
CXX14_CONSTEXPR auto u3 = gen( "{0123456789abcdef0123456789ABCDEF}" );
CXX14_CONSTEXPR auto u4 = gen( "01234567-89AB-CDEF-0123-456789abcdef" );
CXX14_CONSTEXPR auto u5 = gen( "{01234567-89AB-CDEF-0123-456789abcdef}" );

int main()
{
    boost::uuids::uuid const exp1 = {{}};
    boost::uuids::uuid const exp2 = {{ 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef }};

    BOOST_TEST_EQ( u1, exp1 );

    BOOST_TEST_EQ( u2, exp2 );
    BOOST_TEST_EQ( u3, exp2 );
    BOOST_TEST_EQ( u4, exp2 );
    BOOST_TEST_EQ( u5, exp2 );

    return boost::report_errors();
}
