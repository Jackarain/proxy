// Copyright 2026 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#if defined(__GNUC__) || defined(__clang__)
# pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

#include <boost/system/unwrap_and_invoke.hpp>
#include <boost/system/result.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <boost/config/pragma_message.hpp>
#include <boost/config.hpp>

struct X
{
    int v;
};

struct Y
{
    X x1, x2, x3, x4;
};

struct E
{
    int w;
};

#if !defined(BOOST_SYSTEM_HAS_BUILTIN_IS_AGGREGATE) && !( defined(__cpp_lib_is_aggregate) && __cpp_lib_is_aggregate >= 201703L )

BOOST_PRAGMA_MESSAGE("Test skipped, detail::is_aggregate isn't functional")
int main() {}

#else

using namespace boost::system;

int main()
{
    {
        auto r = unwrap_and_construct<Y>( result<X, E>( E{1} ) );
        BOOST_TEST( !r ) && BOOST_TEST_EQ( r.error().w, 1 );
    }

    {
        auto r = unwrap_and_construct<Y>( result<X, E>( X{1} ) );

        BOOST_TEST( r )
            && BOOST_TEST_EQ( r.unsafe_value().x1.v, 1 )
            && BOOST_TEST_EQ( r.unsafe_value().x2.v, 0 )
            && BOOST_TEST_EQ( r.unsafe_value().x3.v, 0 )
            && BOOST_TEST_EQ( r.unsafe_value().x4.v, 0 )
        ;
    }

    //

    {
        auto r = unwrap_and_construct<Y>( result<X, E>( E{1} ), result<X, E>( E{2} ) );
        BOOST_TEST( !r ) && BOOST_TEST_EQ( r.error().w, 1 );
    }

    {
        auto r = unwrap_and_construct<Y>( result<X, E>( X{1} ), result<X, E>( E{2} ) );
        BOOST_TEST( !r ) && BOOST_TEST_EQ( r.error().w, 2 );
    }

    {
        auto r = unwrap_and_construct<Y>( result<X, E>( X{1} ), result<X, E>( X{2} ) );

        BOOST_TEST( r )
            && BOOST_TEST_EQ( r.unsafe_value().x1.v, 1 )
            && BOOST_TEST_EQ( r.unsafe_value().x2.v, 2 )
            && BOOST_TEST_EQ( r.unsafe_value().x3.v, 0 )
            && BOOST_TEST_EQ( r.unsafe_value().x4.v, 0 )
        ;
    }

    //

    {
        auto r = unwrap_and_construct<Y>( result<X, E>( E{1} ), result<X, E>( E{2} ), result<X, E>( E{3} ) );
        BOOST_TEST( !r ) && BOOST_TEST_EQ( r.error().w, 1 );
    }

    {
        auto r = unwrap_and_construct<Y>( result<X, E>( X{1} ), result<X, E>( E{2} ), result<X, E>( E{3} ) );
        BOOST_TEST( !r ) && BOOST_TEST_EQ( r.error().w, 2 );
    }

    {
        auto r = unwrap_and_construct<Y>( result<X, E>( X{1} ), result<X, E>( X{2} ), result<X, E>( E{3} ) );
        BOOST_TEST( !r ) && BOOST_TEST_EQ( r.error().w, 3 );
    }

    {
        auto r = unwrap_and_construct<Y>( result<X, E>( X{1} ), result<X, E>( X{2} ), result<X, E>( X{3} ) );

        BOOST_TEST( r )
            && BOOST_TEST_EQ( r.unsafe_value().x1.v, 1 )
            && BOOST_TEST_EQ( r.unsafe_value().x2.v, 2 )
            && BOOST_TEST_EQ( r.unsafe_value().x3.v, 3 )
            && BOOST_TEST_EQ( r.unsafe_value().x4.v, 0 )
        ;
    }

    //

    {
        auto r = unwrap_and_construct<Y>( result<X, E>( E{1} ), result<X, E>( E{2} ), result<X, E>( E{3} ), result<X, E>( E{4} ) );
        BOOST_TEST( !r ) && BOOST_TEST_EQ( r.error().w, 1 );
    }

    {
        auto r = unwrap_and_construct<Y>( result<X, E>( X{1} ), result<X, E>( E{2} ), result<X, E>( E{3} ), result<X, E>( E{4} ) );
        BOOST_TEST( !r ) && BOOST_TEST_EQ( r.error().w, 2 );
    }

    {
        auto r = unwrap_and_construct<Y>( result<X, E>( X{1} ), result<X, E>( X{2} ), result<X, E>( E{3} ), result<X, E>( E{4} ) );
        BOOST_TEST( !r ) && BOOST_TEST_EQ( r.error().w, 3 );
    }

    {
        auto r = unwrap_and_construct<Y>( result<X, E>( X{1} ), result<X, E>( X{2} ), result<X, E>( X{3} ), result<X, E>( E{4} ) );
        BOOST_TEST( !r ) && BOOST_TEST_EQ( r.error().w, 4 );
    }

    {
        auto r = unwrap_and_construct<Y>( result<X, E>( X{1} ), result<X, E>( X{2} ), result<X, E>( X{3} ), result<X, E>( X{4} ) );

        BOOST_TEST( r )
            && BOOST_TEST_EQ( r.unsafe_value().x1.v, 1 )
            && BOOST_TEST_EQ( r.unsafe_value().x2.v, 2 )
            && BOOST_TEST_EQ( r.unsafe_value().x3.v, 3 )
            && BOOST_TEST_EQ( r.unsafe_value().x4.v, 4 )
        ;
    }

    return boost::report_errors();
}

#endif
