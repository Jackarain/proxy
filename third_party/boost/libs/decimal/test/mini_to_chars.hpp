// Copyright 2022 Peter Dimov
// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_MINI_TO_CHARS_HPP
#define BOOST_DECIMAL_MINI_TO_CHARS_HPP

#include <boost/decimal/detail/config.hpp>

// We need to define these operator<< overloads before
// including boost/core/lightweight_test.hpp, or they
// won't be visible to BOOST_TEST_EQ

#ifdef BOOST_DECIMAL_HAS_INT128

// LCOV_EXCL_START

#include <ostream>

static char* mini_to_chars( char (&buffer)[ 64 ], boost::decimal::detail::builtin_uint128_t v )
{
    char* p = buffer + 64;
    *--p = '\0';

    do
    {
        *--p = "0123456789"[ v % 10 ];
        v /= 10;
    }
    while ( v != 0 );

    return p;
}

std::ostream& operator<<( std::ostream& os, boost::decimal::detail::builtin_uint128_t v )
{
    char buffer[ 64 ];

    os << mini_to_chars( buffer, v );
    return os;
}

std::ostream& operator<<( std::ostream& os, boost::decimal::detail::builtin_int128_t v )
{
    char buffer[ 64 ];
    char* p;

    if( v >= 0 )
    {
        p = mini_to_chars( buffer, static_cast<boost::decimal::detail::builtin_uint128_t>(v) );
    }
    else
    {
        p = mini_to_chars( buffer, -static_cast<boost::decimal::detail::builtin_uint128_t>(v) );
        *--p = '-';
    }

    os << p;
    return os;
}

// LCOV_EXCL_STOP

#endif // #ifdef BOOST_HAS_INT128

#endif //BOOST_DECIMAL_MINI_TO_CHARS_HPP
