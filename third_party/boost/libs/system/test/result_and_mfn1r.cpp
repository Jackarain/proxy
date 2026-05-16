// Copyright 2017, 2021, 2022 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/system/result.hpp>
#include <boost/core/lightweight_test.hpp>

using namespace boost::system;

struct E
{
};

struct E2
{
    E2() {}
    E2( E ) {}
};

struct X
{
    int v_ = 0;
    mutable int g_called_ = 0;

    X( int v ): v_( v ) {}

    result<int, E2> f() const { return v_; }
    result<int, E2> f2() const { return E2(); }

    result<int&, E2> g() { return v_; }
    result<int const&, E2> g2() const { return v_; }

    result<void, E2> h() const { return {}; }
    result<void, E2> h2() const { return E2(); }
};

int main()
{
    {
        result<X, E> r( 1 );

        {
            result<int, E2> r2 = r & &X::f;
            BOOST_TEST( r2.has_value() ) && BOOST_TEST_EQ( r2.unsafe_value(), 1 );
        }

        {
            result<int, E2> r2 = r & &X::f2;
            BOOST_TEST( r2.has_error() );
        }

        {
            result<int&, E2> r2 = r & &X::g;
            BOOST_TEST( r2.has_value() ) && BOOST_TEST_EQ( &r2.unsafe_value(), &r.unsafe_value().v_ );
        }

        {
            result<int const&, E2> r2 = r & &X::g2;
            BOOST_TEST( r2.has_value() ) && BOOST_TEST_EQ( &r2.unsafe_value(), &r.unsafe_value().v_ );
        }

        {
            result<void, E2> r2 = r & &X::h;
            BOOST_TEST( r2.has_value() );
        }

        {
            result<void, E2> r2 = r & &X::h2;
            BOOST_TEST( r2.has_error() );
        }
    }

    {
        result<X, E> const r( 1 );

        {
            result<int, E2> r2 = r & &X::f;
            BOOST_TEST( r2.has_value() ) && BOOST_TEST_EQ( r2.unsafe_value(), 1 );
        }

        {
            result<int, E2> r2 = r & &X::f2;
            BOOST_TEST( r2.has_error() );
        }

        {
            result<int const&, E2> r2 = r & &X::g2;
            BOOST_TEST( r2.has_value() ) && BOOST_TEST_EQ( &r2.unsafe_value(), &r.unsafe_value().v_ );
        }

        {
            result<void, E2> r2 = r & &X::h;
            BOOST_TEST( r2.has_value() );
        }

        {
            result<void, E2> r2 = r & &X::h2;
            BOOST_TEST( r2.has_error() );
        }
    }

    {
        {
            result<int, E2> r2 = result<X, E>( 1 ) & &X::f;
            BOOST_TEST( r2.has_value() ) && BOOST_TEST_EQ( r2.unsafe_value(), 1 );
        }

        {
            result<int, E2> r2 = result<X, E>( 1 ) & &X::f2;
            BOOST_TEST( r2.has_error() );
        }

        {
            result<int&, E2> r2 = result<X, E>( 1 ) & &X::g;
            BOOST_TEST( r2.has_value() );
        }

        {
            result<int const&, E2> r2 = result<X, E>( 1 ) & &X::g2;
            BOOST_TEST( r2.has_value() );
        }

        {
            result<void, E2> r2 = result<X, E>( 1 ) & &X::h;
            BOOST_TEST( r2.has_value() );
        }

        {
            result<void, E2> r2 = result<X, E>( 1 ) & &X::h2;
            BOOST_TEST( r2.has_error() );
        }
    }

    {
        result<X, E> r( in_place_error );

        {
            result<int, E2> r2 = r & &X::f;
            BOOST_TEST( r2.has_error() );
        }

        {
            result<int, E2> r2 = r & &X::f2;
            BOOST_TEST( r2.has_error() );
        }

        {
            result<int&, E2> r2 = r & &X::g;
            BOOST_TEST( r2.has_error() );
        }

        {
            result<int const&, E2> r2 = r & &X::g2;
            BOOST_TEST( r2.has_error() );
        }

        {
            result<void, E2> r2 = r & &X::h;
            BOOST_TEST( r2.has_error() );
        }

        {
            result<void, E2> r2 = r & &X::h2;
            BOOST_TEST( r2.has_error() );
        }
    }

    {
        result<X, E> const r( in_place_error );

        {
            result<int, E2> r2 = r & &X::f;
            BOOST_TEST( r2.has_error() );
        }

        {
            result<int, E2> r2 = r & &X::f2;
            BOOST_TEST( r2.has_error() );
        }

        {
            result<int const&, E2> r2 = r & &X::g2;
            BOOST_TEST( r2.has_error() );
        }

        {
            result<void, E2> r2 = r & &X::h;
            BOOST_TEST( r2.has_error() );
        }

        {
            result<void, E2> r2 = r & &X::h2;
            BOOST_TEST( r2.has_error() );
        }
    }

    {
        {
            result<int, E2> r2 = result<X, E>( in_place_error ) & &X::f;
            BOOST_TEST( r2.has_error() );
        }

        {
            result<int, E2> r2 = result<X, E>( in_place_error ) & &X::f2;
            BOOST_TEST( r2.has_error() );
        }

        {
            result<int&, E2> r2 = result<X, E>( in_place_error ) & &X::g;
            BOOST_TEST( r2.has_error() );
        }

        {
            result<int const&, E2> r2 = result<X, E>( in_place_error ) & &X::g2;
            BOOST_TEST( r2.has_error() );
        }

        {
            result<void, E2> r2 = result<X, E>( in_place_error ) & &X::h;
            BOOST_TEST( r2.has_error() );
        }

        {
            result<void, E2> r2 = result<X, E>( in_place_error ) & &X::h2;
            BOOST_TEST( r2.has_error() );
        }
    }

    return boost::report_errors();
}
