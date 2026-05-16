// Copyright 2026 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/system/error_code.hpp>
#include <boost/system/error_condition.hpp>
#include <boost/config/pragma_message.hpp>
#include <boost/static_assert.hpp>
#include <cstdio>

#if !defined(BOOST_SYSTEM_HAS_CXX20_CONSTEXPR)

BOOST_PRAGMA_MESSAGE("Skipping constexpr test, BOOST_SYSTEM_HAS_CXX20_CONSTEXPR isn't defined")
int main() {}

#else

namespace sys = boost::system;

class user_category: public sys::error_category
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

constexpr sys::error_code ec( 1, s_user_cat );

BOOST_STATIC_ASSERT( ec.failed() );
BOOST_STATIC_ASSERT( ec );
BOOST_STATIC_ASSERT( !!ec );

constexpr sys::error_condition en( 1, s_user_cat );

BOOST_STATIC_ASSERT( en.failed() );
BOOST_STATIC_ASSERT( en );
BOOST_STATIC_ASSERT( !!en );

int main() {}

#endif
