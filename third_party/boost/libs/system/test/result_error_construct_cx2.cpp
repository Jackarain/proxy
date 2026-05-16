// Copyright 2017, 2021, 2026 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/system/result.hpp>
#include <boost/config/pragma_message.hpp>
#include <boost/static_assert.hpp>
#include <cstdio>

#if !defined(BOOST_SYSTEM_HAS_CXX20_CONSTEXPR)

BOOST_PRAGMA_MESSAGE("Skipping constexpr test, BOOST_SYSTEM_HAS_CXX20_CONSTEXPR isn't defined")

#else

using namespace boost::system;

class user_category: public error_category
{
public:

    constexpr virtual const char* name() const noexcept
    {
        return "user";
    }

    virtual std::string message( int ev ) const
    {
        char buffer[ 256 ];
        std::snprintf( buffer, sizeof( buffer ), "user message %d", ev );

        return buffer;
    }
};

static constexpr user_category s_user_cat;

int main()
{
    {
        constexpr result<int> r( 1, s_user_cat );

        BOOST_STATIC_ASSERT( !r.has_value() );
        BOOST_STATIC_ASSERT( r.has_error() );

        BOOST_STATIC_ASSERT( r.error() == error_code( 1, s_user_cat ) );
    }

    {
        constexpr result<error_code> r( in_place_error, 2, s_user_cat );

        BOOST_STATIC_ASSERT( !r.has_value() );
        BOOST_STATIC_ASSERT( r.has_error() );

        BOOST_STATIC_ASSERT( r.error() == error_code( 2, s_user_cat ) );
    }

    {
        constexpr result<void> r( 3, s_user_cat );

        BOOST_STATIC_ASSERT( !r.has_value() );
        BOOST_STATIC_ASSERT( r.has_error() );

        BOOST_STATIC_ASSERT( r.error() == error_code( 3, s_user_cat ) );
    }

    {
        constexpr result<void> r( in_place_error, 4, s_user_cat );

        BOOST_STATIC_ASSERT( !r.has_value() );
        BOOST_STATIC_ASSERT( r.has_error() );

        BOOST_STATIC_ASSERT( r.error() == error_code( 4, s_user_cat ) );
    }
}

#endif
