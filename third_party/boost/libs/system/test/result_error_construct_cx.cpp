// Copyright 2017, 2021, 2026 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/system/result.hpp>
#include <boost/config/pragma_message.hpp>
#include <boost/static_assert.hpp>
#include <boost/config.hpp>

#if !defined(BOOST_SYSTEM_HAS_CONSTEXPR)

BOOST_PRAGMA_MESSAGE("Skipping constexpr test, BOOST_SYSTEM_HAS_CONSTEXPR isn't defined")

#elif defined(BOOST_CLANG_VERSION) && BOOST_CLANG_VERSION < 60000

BOOST_PRAGMA_MESSAGE("Skipping constexpr test, BOOST_CLANG_VERSION < 60000")

#else

using namespace boost::system;

int main()
{
    {
        constexpr auto ec = make_error_code( errc::invalid_argument );

        constexpr result<int> r( ec );

        BOOST_STATIC_ASSERT( !r.has_value() );
        BOOST_STATIC_ASSERT( r.has_error() );

        BOOST_STATIC_ASSERT( r.error() == ec );
    }

    {
        constexpr result<int> r( EINVAL, generic_category() );

        BOOST_STATIC_ASSERT( !r.has_value() );
        BOOST_STATIC_ASSERT( r.has_error() );

        BOOST_STATIC_ASSERT( r.error() == error_code( EINVAL, generic_category() ) );
    }

    {
        constexpr auto ec = make_error_code( errc::invalid_argument );

        constexpr result<error_code> r( in_place_error, ec );

        BOOST_STATIC_ASSERT( !r.has_value() );
        BOOST_STATIC_ASSERT( r.has_error() );

        BOOST_STATIC_ASSERT( r.error() == ec );
    }

    {
        constexpr result<error_code> r( in_place_error, EINVAL, generic_category() );

        BOOST_STATIC_ASSERT( !r.has_value() );
        BOOST_STATIC_ASSERT( r.has_error() );

        BOOST_STATIC_ASSERT( r.error() == error_code( EINVAL, generic_category() ) );
    }

    {
        constexpr auto ec = make_error_code( errc::invalid_argument );

        constexpr result<void> r( ec );

        BOOST_STATIC_ASSERT( !r.has_value() );
        BOOST_STATIC_ASSERT( r.has_error() );

        BOOST_STATIC_ASSERT( r.error() == ec );
    }

    {
        constexpr auto ec = make_error_code( errc::invalid_argument );

        constexpr result<void> r = ec;

        BOOST_STATIC_ASSERT( !r.has_value() );
        BOOST_STATIC_ASSERT( r.has_error() );

        BOOST_STATIC_ASSERT( r.error() == ec );
    }

    {
        constexpr result<void> r( EINVAL, generic_category() );

        BOOST_STATIC_ASSERT( !r.has_value() );
        BOOST_STATIC_ASSERT( r.has_error() );

        BOOST_STATIC_ASSERT( r.error() == error_code( EINVAL, generic_category() ) );
    }
}

#endif
