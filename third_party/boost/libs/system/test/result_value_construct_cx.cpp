// Copyright 2017, 2021, 2026 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/system/result.hpp>
#include <boost/config/pragma_message.hpp>
#include <boost/static_assert.hpp>

#if !defined(BOOST_SYSTEM_HAS_CONSTEXPR)

BOOST_PRAGMA_MESSAGE("Skipping constexpr test, BOOST_SYSTEM_HAS_CONSTEXPR isn't defined")

#else

using namespace boost::system;

int main()
{
    {
        constexpr result<int> r( 0 );

        BOOST_STATIC_ASSERT( r.has_value() );
        BOOST_STATIC_ASSERT( !r.has_error() );

        BOOST_STATIC_ASSERT( r.value() == 0 );
        BOOST_STATIC_ASSERT( *r == 0 );
    }

    {
        constexpr result<int> r( in_place_value, 1 );

        BOOST_STATIC_ASSERT( r.has_value() );
        BOOST_STATIC_ASSERT( !r.has_error() );

        BOOST_STATIC_ASSERT( r.value() == 1 );
        BOOST_STATIC_ASSERT( *r == 1 );
    }

    {
        constexpr result<void> r( in_place_value );

        BOOST_STATIC_ASSERT( r.has_value() );
        BOOST_STATIC_ASSERT( !r.has_error() );
    }
}

#endif
