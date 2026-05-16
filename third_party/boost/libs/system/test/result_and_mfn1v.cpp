// Copyright 2017, 2021, 2022 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/system/result.hpp>
#include <boost/core/lightweight_test.hpp>

using namespace boost::system;

struct X
{
    int v_ = 0;
    mutable int g_called_ = 0;

    X( int v ): v_( v ) {}

    int f() const { return v_; }

    void g() const { ++g_called_; }

    int& h() { return v_; }
    int const& h2() const { return v_; }
};

struct E
{
};

int main()
{
    {
        result<X> r( 1 );
        result<int> r2 = r & &X::f;

        BOOST_TEST( r2.has_value() ) && BOOST_TEST_EQ( *r2, 1 );
    }

    {
        result<X> const r( 1 );
        result<int> r2 = r & &X::f;

        BOOST_TEST( r2.has_value() ) && BOOST_TEST_EQ( *r2, 1 );
    }

    {
        result<int> r2 = result<X>( 1 ) & &X::f;

        BOOST_TEST( r2.has_value() ) && BOOST_TEST_EQ( *r2, 1 );
    }

    {
        result<X, E> r( in_place_error );
        result<int, E> r2 = r & &X::f;

        BOOST_TEST( r2.has_error() );
    }

    {
        result<X, E> const r( in_place_error );
        result<int, E> r2 = r & &X::f;

        BOOST_TEST( r2.has_error() );
    }

    {
        result<int, E> r2 = result<X, E>( in_place_error ) & &X::f;

        BOOST_TEST( r2.has_error() );
    }

    {
        result<X> r( 1 );
        result<int&> r2 = r & &X::v_;

        BOOST_TEST( r2.has_value() ) && BOOST_TEST_EQ( &*r2, &r->v_ );
    }

    {
        result<X> const r( 1 );
        result<int const&> r2 = r & &X::v_;

        BOOST_TEST( r2.has_value() ) && BOOST_TEST_EQ( &*r2, &r->v_ );
    }

    {
        result<int> r2 = result<X>( 1 ) & &X::v_;

        BOOST_TEST( r2.has_value() ) && BOOST_TEST_EQ( *r2, 1 );
    }

    {
        result<X, E> r( in_place_error );
        result<int&, E> r2 = r & &X::v_;

        BOOST_TEST( r2.has_error() );
    }

    {
        result<X, E> const r( in_place_error );
        result<int const&, E> r2 = r & &X::v_;

        BOOST_TEST( r2.has_error() );
    }

    {
        result<int, E> r2 = result<X, E>( in_place_error ) & &X::v_;

        BOOST_TEST( r2.has_error() );
    }

    {
        X x( 1 );

        result<X&> r( x );
        result<int&> r2 = r & &X::v_;

        BOOST_TEST( r2.has_value() ) && BOOST_TEST_EQ( &*r2, &x.v_ );
    }

    {
        X const x( 1 );

        result<X const&> r( x );
        result<int const&> r2 = r & &X::v_;

        BOOST_TEST( r2.has_value() ) && BOOST_TEST_EQ( &*r2, &x.v_ );
    }

    {
        X x( 1 );

        result<int&> r2 = result<X&>( x ) & &X::v_;

        BOOST_TEST( r2.has_value() ) && BOOST_TEST_EQ( &*r2, &x.v_ );
    }

    {
        X const x( 1 );

        result<int const&> r2 = result<X const&>( x ) & &X::v_;

        BOOST_TEST( r2.has_value() ) && BOOST_TEST_EQ( &*r2, &x.v_ );
    }

    {
        result<X&, E> r( in_place_error );
        result<int&, E> r2 = r & &X::v_;

        BOOST_TEST( r2.has_error() );
    }

    {
        result<X const&, E> const r( in_place_error );
        result<int const&, E> r2 = r & &X::v_;

        BOOST_TEST( r2.has_error() );
    }

    {
        result<int&, E> r2 = result<X&, E>( in_place_error ) & &X::v_;

        BOOST_TEST( r2.has_error() );
    }

    {
        result<int const&, E> r2 = result<X const&, E>( in_place_error ) & &X::v_;

        BOOST_TEST( r2.has_error() );
    }

    {
        result<X> r( 1 );
        result<void> r2 = r & &X::g;

        BOOST_TEST( r2.has_value() );
        BOOST_TEST_EQ( r->g_called_, 1 );
    }

    {
        result<X> const r( 1 );
        result<void> r2 = r & &X::g;

        BOOST_TEST( r2.has_value() );
        BOOST_TEST_EQ( r->g_called_, 1 );
    }

    {
        result<void> r2 = result<X>( 1 ) & &X::g;

        BOOST_TEST( r2.has_value() );
    }

    {
        result<X, E> r( in_place_error );
        result<void, E> r2 = r & &X::g;

        BOOST_TEST( r2.has_error() );
    }

    {
        result<X, E> const r( in_place_error );
        result<void, E> r2 = r & &X::g;

        BOOST_TEST( r2.has_error() );
    }

    {
        result<void, E> r2 = result<X, E>( in_place_error ) & &X::g;

        BOOST_TEST( r2.has_error() );
    }

    {
        result<X> r( 1 );
        result<int&> r2 = r & &X::h;

        BOOST_TEST( r2.has_value() ) && BOOST_TEST_EQ( *r2, 1 );
    }

    {
        result<X> const r( 1 );
        result<int const&> r2 = r & &X::h2;

        BOOST_TEST( r2.has_value() ) && BOOST_TEST_EQ( *r2, 1 );
    }

    {
        result<int> r2 = result<X>( 1 ) & &X::h2;

        BOOST_TEST( r2.has_value() ) && BOOST_TEST_EQ( *r2, 1 );
    }

    {
        result<X, E> r( in_place_error );
        result<int&, E> r2 = r & &X::h;

        BOOST_TEST( r2.has_error() );
    }

    {
        result<X, E> const r( in_place_error );
        result<int const&, E> r2 = r & &X::h2;

        BOOST_TEST( r2.has_error() );
    }

    {
        result<int, E> r2 = result<X, E>( in_place_error ) & &X::h2;

        BOOST_TEST( r2.has_error() );
    }

    return boost::report_errors();
}
