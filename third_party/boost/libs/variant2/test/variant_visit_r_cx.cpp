// Copyright 2017, 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/variant2/variant.hpp>
using namespace boost::variant2;

#if !defined(BOOST_MP11_HAS_CXX14_CONSTEXPR)

#include <boost/config/pragma_message.hpp>

BOOST_PRAGMA_MESSAGE("Skipping constexpr visit test because BOOST_MP11_HAS_CXX14_CONSTEXPR is not defined")

int main() {}

#else

struct X
{
    int v;
    constexpr operator int() const { return v; }
};

struct F
{
    template<class T> constexpr T operator()( T x ) const
    {
        return x;
    }
};

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

int main()
{
    {
        constexpr variant<int> v( 1 );
        STATIC_ASSERT( visit<int>( F(), v ) == 1 );
    }

    {
        constexpr variant<char, int> v( 'a' );
        STATIC_ASSERT( visit<int>( F(), v ) == 'a' );
    }

    {
        constexpr variant<char, int, X> v( X{2} );
        STATIC_ASSERT( visit<int>( F(), v ) == 2 );
    }
}

#endif
