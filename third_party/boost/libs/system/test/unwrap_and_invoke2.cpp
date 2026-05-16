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

struct E
{
    int w;
};

X f1( X x1 )
{
    return x1;
}

X f2( X x1, X x2 )
{
    return X{ x1.v + x2.v };
}

X f3( X x1, X x2, X x3 )
{
    return X{ x1.v + x2.v + x3.v };
}

X f4( X x1, X x2, X x3, X x4 )
{
    return X{ x1.v + x2.v + x3.v + x4.v };
}

int main()
{
    {
        auto r = unwrap_and_invoke( f1, result<X, E>( E{1} ) );
        BOOST_TEST( !r ) && BOOST_TEST_EQ( r.error().w, 1 );
    }

    {
        auto r = unwrap_and_invoke( f1, result<X, E>( X{1} ) );
        BOOST_TEST( r ) && BOOST_TEST_EQ( r.unsafe_value().v, 1 );
    }

    //

    {
        auto r = unwrap_and_invoke( f2, result<X, E>( E{1} ), result<X, E>( E{2} ) );
        BOOST_TEST( !r ) && BOOST_TEST_EQ( r.error().w, 1 );
    }

    {
        auto r = unwrap_and_invoke( f2, result<X, E>( X{1} ), result<X, E>( E{2} ) );
        BOOST_TEST( !r ) && BOOST_TEST_EQ( r.error().w, 2 );
    }

    {
        auto r = unwrap_and_invoke( f2, result<X, E>( X{1} ), result<X, E>( X{2} ) );
        BOOST_TEST( r ) && BOOST_TEST_EQ( r.unsafe_value().v, 3 );
    }

    //

    {
        auto r = unwrap_and_invoke( f3, result<X, E>( E{1} ), result<X, E>( E{2} ), result<X, E>( E{3} ) );
        BOOST_TEST( !r ) && BOOST_TEST_EQ( r.error().w, 1 );
    }

    {
        auto r = unwrap_and_invoke( f3, result<X, E>( X{1} ), result<X, E>( E{2} ), result<X, E>( E{3} ) );
        BOOST_TEST( !r ) && BOOST_TEST_EQ( r.error().w, 2 );
    }

    {
        auto r = unwrap_and_invoke( f3, result<X, E>( X{1} ), result<X, E>( X{2} ), result<X, E>( E{3} ) );
        BOOST_TEST( !r ) && BOOST_TEST_EQ( r.error().w, 3 );
    }

    {
        auto r = unwrap_and_invoke( f3, result<X, E>( X{1} ), result<X, E>( X{2} ), result<X, E>( X{3} ) );
        BOOST_TEST( r ) && BOOST_TEST_EQ( r.unsafe_value().v, 6 );
    }

    //

    {
        auto r = unwrap_and_invoke( f4, result<X, E>( E{1} ), result<X, E>( E{2} ), result<X, E>( E{3} ), result<X, E>( E{4} ) );
        BOOST_TEST( !r ) && BOOST_TEST_EQ( r.error().w, 1 );
    }

    {
        auto r = unwrap_and_invoke( f4, result<X, E>( X{1} ), result<X, E>( E{2} ), result<X, E>( E{3} ), result<X, E>( E{4} ) );
        BOOST_TEST( !r ) && BOOST_TEST_EQ( r.error().w, 2 );
    }

    {
        auto r = unwrap_and_invoke( f4, result<X, E>( X{1} ), result<X, E>( X{2} ), result<X, E>( E{3} ), result<X, E>( E{4} ) );
        BOOST_TEST( !r ) && BOOST_TEST_EQ( r.error().w, 3 );
    }

    {
        auto r = unwrap_and_invoke( f4, result<X, E>( X{1} ), result<X, E>( X{2} ), result<X, E>( X{3} ), result<X, E>( E{4} ) );
        BOOST_TEST( !r ) && BOOST_TEST_EQ( r.error().w, 4 );
    }

    {
        auto r = unwrap_and_invoke( f4, result<X, E>( X{1} ), result<X, E>( X{2} ), result<X, E>( X{3} ), result<X, E>( X{4} ) );
        BOOST_TEST( r ) && BOOST_TEST_EQ( r.unsafe_value().v, 10 );
    }

    return boost::report_errors();
}
