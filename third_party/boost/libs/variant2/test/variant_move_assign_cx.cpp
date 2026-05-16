// Copyright 2017, 2026 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/variant2/variant.hpp>
#include <boost/config.hpp>
#include <boost/config/pragma_message.hpp>
#include <utility>

#if defined(BOOST_NO_CXX14_CONSTEXPR)

BOOST_PRAGMA_MESSAGE( "Test skipped because BOOST_NO_CXX14_CONSTEXPR is defined" )
int main() {}

#elif defined(BOOST_GCC) && BOOST_GCC < 70000

BOOST_PRAGMA_MESSAGE( "Test skipped because BOOST_GCC < 70000" )
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

template<class V, std::size_t I, class A> constexpr variant_alternative_t<I, V> test( A&& a )
{
    V v;

    v = std::forward<A>(a);

    return get<I>(v);
}

int main()
{
    {
        constexpr auto w = test<variant<int>, 0>( variant<int>( 1 ) );
        STATIC_ASSERT( w == 1 );
    }

    {
        constexpr auto w = test<variant<X>, 0>( variant<X>( 1 ) );
        STATIC_ASSERT( w == 1 );
    }

#if defined( BOOST_LIBSTDCXX_VERSION ) && BOOST_LIBSTDCXX_VERSION < 50000
#else

    {
        constexpr auto w = test<variant<Y>, 0>( variant<Y>( 1 ) );
        STATIC_ASSERT( w == 1 );
    }

#endif

    {
        constexpr auto w = test<variant<int, float>, 0>( variant<int, float>( 1 ) );
        STATIC_ASSERT( w == 1 );
    }

    {
        constexpr auto w = test<variant<int, float>, 1>( variant<int, float>( 3.0f ) );
        STATIC_ASSERT( w == 3.0f );
    }

    {
        constexpr auto w = test<variant<int, int, float>, 2>( variant<int, int, float>( 3.0f ) );
        STATIC_ASSERT( w == 3.0f );
    }

    {
        constexpr auto w = test<variant<E, E, X>, 2>( variant<E, E, X>( 1 ) );
        STATIC_ASSERT( w == 1 );
    }

    {
        constexpr auto w = test<variant<int, int, float, float, X>, 4>( variant<int, int, float, float, X>( X(1) ) );
        STATIC_ASSERT( w == 1 );
    }

#if defined( BOOST_LIBSTDCXX_VERSION ) && BOOST_LIBSTDCXX_VERSION < 50000
#else

    {
        constexpr auto w = test<variant<E, E, Y>, 2>( variant<E, E, Y>( 1 ) );
        STATIC_ASSERT( w == 1 );
    }

    {
        constexpr auto w = test<variant<int, int, float, float, Y>, 4>( variant<int, int, float, float, Y>( Y(1) ) );
        STATIC_ASSERT( w == 1 );
    }

#endif
}

#endif
