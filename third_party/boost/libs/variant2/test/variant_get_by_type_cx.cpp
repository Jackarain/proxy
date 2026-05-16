// Copyright 2017, 2026 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/variant2/variant.hpp>
#include <boost/mp11.hpp>
#include <boost/config.hpp>
#include <boost/config/pragma_message.hpp>

#if !defined(BOOST_MP11_HAS_CXX14_CONSTEXPR)

BOOST_PRAGMA_MESSAGE("Test skipped because BOOST_MP11_HAS_CXX14_CONSTEXPR is not defined")
int main() {}

#else

using namespace boost::variant2;

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

#if BOOST_WORKAROUND(BOOST_GCC, < 50000)
# define STATIC_ASSERT_IF(...)
#else
# define STATIC_ASSERT_IF(...) static_assert(__VA_ARGS__, #__VA_ARGS__)
#endif

int main()
{
    {
        constexpr variant<int> v;

        STATIC_ASSERT( get<int>(v) == 0 );

        STATIC_ASSERT_IF( get_if<int>(&v) == &get<int>(v) );
    }

    {
        constexpr variant<int> v( 1 );

        STATIC_ASSERT( get<int>(v) == 1 );

        STATIC_ASSERT_IF( get_if<int>(&v) == &get<int>(v) );
    }

    {
        constexpr variant<int, float> v;

        STATIC_ASSERT( get<int>(v) == 0 );

        STATIC_ASSERT_IF( get_if<int>(&v) == &get<int>(v) );
        STATIC_ASSERT_IF( get_if<float>(&v) == nullptr );
    }

    {
        constexpr variant<int, float> v( 1 );

        STATIC_ASSERT( get<int>(v) == 1 );

        STATIC_ASSERT_IF( get_if<int>(&v) == &get<int>(v) );
        STATIC_ASSERT_IF( get_if<float>(&v) == nullptr );
    }

    {
        constexpr variant<int, float> v( 3.14f );

        STATIC_ASSERT( get<float>(v) == (float)3.14f ); // see FLT_EVAL_METHOD

        STATIC_ASSERT_IF( get_if<int>(&v) == nullptr );
        STATIC_ASSERT_IF( get_if<float>(&v) == &get<float>(v) );
    }

    {
        constexpr variant<int, float, float> v;

        STATIC_ASSERT( get<int>(v) == 0 );

        STATIC_ASSERT_IF( get_if<int>(&v) == &get<int>(v) );
        STATIC_ASSERT_IF( get_if<float>(&v) == nullptr );
    }

    {
        constexpr variant<int, float, float> v( 1 );

        STATIC_ASSERT( get<int>(v) == 1 );

        STATIC_ASSERT_IF( get_if<int>(&v) == &get<int>(v) );
        STATIC_ASSERT_IF( get_if<float>(&v) == nullptr );
    }

    {
        constexpr variant<int, int, float> v( 3.14f );

        STATIC_ASSERT( get<float>(v) == (float)3.14f );

        STATIC_ASSERT_IF( get_if<int>(&v) == nullptr );
        STATIC_ASSERT_IF( get_if<float>(&v) == &get<float>(v) );
    }
}

#endif
