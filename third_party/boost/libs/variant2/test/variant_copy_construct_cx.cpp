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

struct X
{
    int v;
    X() = default;
    constexpr X( int v ): v( v ) {}
    constexpr operator int() const { return v; }
};

struct Y
{
    int v;
    constexpr Y(): v() {}
    constexpr Y( int v ): v( v ) {}
    constexpr operator int() const { return v; }
};

enum E
{
    v
};

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

template<std::size_t I, class V> constexpr variant_alternative_t<I, V> test( V const v )
{
    return get<I>( v );
}

int main()
{
    {
        constexpr variant<int> v( 1 );
        constexpr auto w = test<0>( v );
        STATIC_ASSERT( w == 1 );
    }

    {
        constexpr variant<X> v( 1 );
        constexpr auto w = test<0>( v );
        STATIC_ASSERT( w == 1 );
    }

#if defined( BOOST_LIBSTDCXX_VERSION ) && BOOST_LIBSTDCXX_VERSION < 50000
#else

    {
        constexpr variant<Y> v( 1 );
        constexpr auto w = test<0>( v );
        STATIC_ASSERT( w == 1 );
    }

#endif

    {
        constexpr variant<int, float> v( 1 );
        constexpr auto w = test<0>( v );
        STATIC_ASSERT( w == 1 );
    }

    {
        constexpr variant<int, float> v( 3.0f );
        constexpr auto w = test<1>( v );
        STATIC_ASSERT( w == 3.0f );
    }

    {
        constexpr variant<int, int, float> v( 3.0f );
        constexpr auto w = test<2>( v );
        STATIC_ASSERT( w == 3.0f );
    }

    {
        constexpr variant<E, E, X> v( 1 );
        constexpr auto w = test<2>( v );
        STATIC_ASSERT( w == 1 );
    }

    {
        constexpr variant<int, int, float, float, X> v( X(1) );
        constexpr auto w = test<4>( v );
        STATIC_ASSERT( w == 1 );
    }

#if defined( BOOST_LIBSTDCXX_VERSION ) && BOOST_LIBSTDCXX_VERSION < 50000
#else

    {
        constexpr variant<E, E, Y> v( 1 );
        constexpr auto w = test<2>( v );
        STATIC_ASSERT( w == 1 );
    }

    {
        constexpr variant<int, int, float, float, Y> v( Y(1) );
        constexpr auto w = test<4>( v );
        STATIC_ASSERT( w == 1 );
    }

#endif
}

#endif
