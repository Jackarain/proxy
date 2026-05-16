// Copyright 2026 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/system/unwrap_and_invoke.hpp>
#include <boost/system/result.hpp>
#include <boost/core/lightweight_test_trait.hpp>

using namespace boost::system;

struct X
{
    int v;
};

struct Y
{
    int v;

    explicit Y( X a1 = {0}, X a2 = {0}, X a3 = {0}, X a4 = {0} ): v( a1.v + a2.v + a3.v + a4.v )
    {
    }
};

struct E
{
    int w;
};

int main()
{
    {
        auto r = unwrap_and_construct<Y>( result<X, E>( E{1} ) );
        BOOST_TEST( !r ) && BOOST_TEST_EQ( r.error().w, 1 );
    }

    {
        auto r = unwrap_and_construct<Y>( result<X, E>( X{1} ) );
        BOOST_TEST( r ) && BOOST_TEST_EQ( r.unsafe_value().v, 1 );
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
        BOOST_TEST( r ) && BOOST_TEST_EQ( r.unsafe_value().v, 3 );
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
        BOOST_TEST( r ) && BOOST_TEST_EQ( r.unsafe_value().v, 6 );
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
        BOOST_TEST( r ) && BOOST_TEST_EQ( r.unsafe_value().v, 10 );
    }

    return boost::report_errors();
}
