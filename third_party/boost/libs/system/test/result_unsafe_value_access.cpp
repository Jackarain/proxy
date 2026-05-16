// Copyright 2017, 2021, 2022, 2026 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/system/result.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>

using namespace boost::system;

struct X
{
    int v_;

    explicit X( int v ): v_( v ) {}

    X( X const& ) = delete;
    X& operator=( X const& ) = delete;
};

int main()
{
    {
        result<int> r;

        BOOST_TEST( r.has_value() );
        BOOST_TEST_EQ( r.unsafe_value(), 0 );
    }

    {
        result<int> const r;

        BOOST_TEST( r.has_value() );
        BOOST_TEST_EQ( r.unsafe_value(), 0 );
    }

    {
        BOOST_TEST( result<int>().has_value() );
        BOOST_TEST_EQ( result<int>().unsafe_value(), 0 );
    }

    {
        result<int> r( 1 );

        BOOST_TEST( r.has_value() );
        BOOST_TEST_EQ( r.unsafe_value(), 1 );
    }

    {
        result<int> const r( 1 );

        BOOST_TEST( r.has_value() );
        BOOST_TEST_EQ( r.unsafe_value(), 1 );
    }

    {
        BOOST_TEST( result<int>( 1 ).has_value() );
        BOOST_TEST_EQ( result<int>( 1 ).unsafe_value(), 1 );
    }

    //

    {
        result<X> r( 1 );

        BOOST_TEST( r.has_value() );
        BOOST_TEST_EQ( r.unsafe_value().v_, 1 );
    }

    {
        result<X> const r( 1 );

        BOOST_TEST( r.has_value() );
        BOOST_TEST_EQ( r.unsafe_value().v_, 1 );
    }

    {
        BOOST_TEST( result<X>( 1 ).has_value() );
        BOOST_TEST_EQ( result<X>( 1 ).unsafe_value().v_, 1 );
    }

    //

    {
        result<void> r;

        BOOST_TEST( r.has_value() );
        BOOST_TEST_NO_THROW( r.unsafe_value() );
    }

    {
        result<void> const r;

        BOOST_TEST( r.has_value() );
        BOOST_TEST_NO_THROW( r.unsafe_value() );
    }

    {
        BOOST_TEST( result<void>().has_value() );
        BOOST_TEST_NO_THROW( result<void>().unsafe_value() );
    }

    //

    {
        int x1 = 1;
        result<int&> r( x1 );

        BOOST_TEST( r.has_value() );

        BOOST_TEST_EQ( r.unsafe_value(), 1 );
        BOOST_TEST_EQ( &r.unsafe_value(), &x1 );
    }

    {
        int x1 = 1;
        result<int&> const r( x1 );

        BOOST_TEST( r.has_value() );

        BOOST_TEST_EQ( r.unsafe_value(), 1 );
        BOOST_TEST_EQ( &r.unsafe_value(), &x1 );
    }

    {
        int x1 = 1;

        BOOST_TEST( result<int&>( x1 ).has_value() );

        BOOST_TEST_EQ( result<int&>( x1 ).unsafe_value(), 1 );
        BOOST_TEST_EQ( &result<int&>( x1 ).unsafe_value(), &x1 );
    }

    //

    return boost::report_errors();
}
