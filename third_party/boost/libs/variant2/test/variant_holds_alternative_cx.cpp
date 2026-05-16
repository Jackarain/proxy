// Copyright 2017, 2026 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/variant2/variant.hpp>
#include <boost/config.hpp>
#include <boost/config/pragma_message.hpp>

#if defined(BOOST_MSVC) && BOOST_MSVC < 1910

BOOST_PRAGMA_MESSAGE( "Test skipped because BOOST_MSVC < 1910" )
int main() {}

#else

using namespace boost::variant2;

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

int main()
{
    {
        constexpr variant<int> v;
        STATIC_ASSERT( holds_alternative<int>( v ) );
    }

    {
        constexpr variant<int, float> v;
        STATIC_ASSERT( holds_alternative<int>( v ) );
        STATIC_ASSERT( !holds_alternative<float>( v ) );
    }

    {
        constexpr variant<int, float> v( 3.14f );
        STATIC_ASSERT( !holds_alternative<int>( v ) );
        STATIC_ASSERT( holds_alternative<float>( v ) );
    }

    {
        constexpr variant<int, float, float> v;
        STATIC_ASSERT( holds_alternative<int>( v ) );
        STATIC_ASSERT( !holds_alternative<float>( v ) );
    }

    {
        constexpr variant<int, int, float> v( 3.14f );
        STATIC_ASSERT( !holds_alternative<int>( v ) );
        STATIC_ASSERT( holds_alternative<float>( v ) );
    }

    {
        constexpr variant<int, int, float> v;
        STATIC_ASSERT( holds_alternative<int>( v ) );
        STATIC_ASSERT( !holds_alternative<float>( v ) );
    }
}

#endif
