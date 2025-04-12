// Copyright 2017, 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/variant2/variant.hpp>
#include <boost/config/pragma_message.hpp>

#if !( defined(__cpp_constexpr) && __cpp_constexpr >= 201603L )

BOOST_PRAGMA_MESSAGE("Skipping constexpr visit test because __cpp_constexpr < 201603L")
int main() {}

#elif !defined(BOOST_MP11_HAS_CXX14_CONSTEXPR)

BOOST_PRAGMA_MESSAGE("Skipping constexpr visit test because BOOST_MP11_HAS_CXX14_CONSTEXPR is not defined")
int main() {}

#else

using namespace boost::variant2;

struct F
{
    constexpr int operator()( int x1, int x2 ) const
    {
        return x1 + x2;
    }

    constexpr int operator()( int x1, int x2, int x3 ) const
    {
        return x1 + x2 + x3;
    }
};

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

int main()
{
    {
        constexpr variant<char, int> v1( 1 );
        constexpr variant<char, int> v2( '2' );
        STATIC_ASSERT( visit( F(), v1, v2 ) == 1 + '2' );
    }

    {
        constexpr variant<char, int> v1( 1 );
        constexpr variant<char, int> v2( 2 );
        constexpr variant<char, int> v3( '3' );
        STATIC_ASSERT( visit( F(), v1, v2, v3 ) == 1 + 2 + '3' );
    }
}

#endif
