// Copyright 2017, 2026 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/variant2/variant.hpp>
#include <boost/config.hpp>
#include <boost/config/workaround.hpp>
#include <boost/config/pragma_message.hpp>

#if defined(BOOST_MSVC) && BOOST_MSVC < 1910

BOOST_PRAGMA_MESSAGE( "Test skipped because BOOST_MSVC < 1910" )
int main() {}

#else

using namespace boost::variant2;

struct X
{
    constexpr X() = default;
    constexpr explicit X(int, int) {}
    template<class T> X( in_place_type_t<T> ) = delete;
};

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

int main()
{
    {
        constexpr variant<int> v( in_place_type_t<int>{} );

        STATIC_ASSERT( v.index() == 0 );
        STATIC_ASSERT( get<0>(v) == 0 );

        STATIC_ASSERT( holds_alternative<int>(v) );
    }

    {
        constexpr variant<X> v( in_place_type_t<X>{} );

        STATIC_ASSERT( v.index() == 0 );

        STATIC_ASSERT( holds_alternative<X>(v) );
    }

    {
        constexpr variant<int> v( in_place_type_t<int>{}, 1 );

        STATIC_ASSERT( v.index() == 0 );
        STATIC_ASSERT( get<0>(v) == 1 );

        STATIC_ASSERT( holds_alternative<int>(v) );
    }

    {
        constexpr variant<int, float> v( in_place_type_t<int>{} );

        STATIC_ASSERT( v.index() == 0 );
        STATIC_ASSERT( get<0>(v) == 0 );

        STATIC_ASSERT( holds_alternative<int>(v) );
    }

    {
        constexpr variant<int, float> v( in_place_type_t<int>{}, 1 );

        STATIC_ASSERT( v.index() == 0 );
        STATIC_ASSERT( get<0>(v) == 1 );

        STATIC_ASSERT( holds_alternative<int>(v) );
    }

    {
        constexpr variant<int, float> v( in_place_type_t<float>{} );

        STATIC_ASSERT( v.index() == 1 );
        STATIC_ASSERT( get<1>(v) == 0 );

        STATIC_ASSERT( holds_alternative<float>(v) );
    }

    {
        constexpr variant<int, float> v( in_place_type_t<float>{}, 3.14f );

        STATIC_ASSERT( v.index() == 1 );
        STATIC_ASSERT( get<1>(v) == (float)3.14f ); // see FLT_EVAL_METHOD

        STATIC_ASSERT( holds_alternative<float>(v) );
    }

    {
        constexpr variant<int, int, float, X> v( in_place_type_t<float>{}, 3.14f );

        STATIC_ASSERT( v.index() == 2 );
        STATIC_ASSERT( get<2>(v) == (float)3.14f );

        STATIC_ASSERT( holds_alternative<float>(v) );
    }

    {
        constexpr variant<int, int, float, float, X> v( in_place_type_t<X>{} );

        STATIC_ASSERT( v.index() == 4 );

        STATIC_ASSERT( holds_alternative<X>(v) );
    }

#if BOOST_WORKAROUND(BOOST_GCC, >= 100000 && BOOST_GCC < 120000)

    // no idea why this fails on g++ 10/11

#else

    {
        constexpr variant<int, int, float, float, X> v( in_place_type_t<X>{}, 0, 0 );

        STATIC_ASSERT( v.index() == 4 );

        STATIC_ASSERT( holds_alternative<X>(v) );
    }

#endif
}

#endif
