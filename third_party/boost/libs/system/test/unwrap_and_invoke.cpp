// Copyright 2026 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/system/unwrap_and_invoke.hpp>
#include <boost/system/result.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <memory>

using namespace boost::system;

int f( int x, int y )
{
    return x + y;
}

struct X
{
    int x_ = 0;

    explicit X( int x ): x_( x )
    {
    }

    int f( int y ) const
    {
        return x_ + y;
    }

    int g( std::unique_ptr<X> p ) const
    {
        return x_ + p->x_;
    }
};

template<class T, class... A> std::unique_ptr<T> make_unique( A&&... a )
{
    return std::unique_ptr<T>( new T( std::forward<A>(a)... ) );
}

int main()
{
    {
        result<int> a1( 1 );

        auto r = unwrap_and_invoke( f, a1, 2 );
        BOOST_TEST( r ) && BOOST_TEST_EQ( *r, 3 );
    }

    {
        result<int> const a1( 1 );

        auto r = unwrap_and_invoke( f, a1, 2 );
        BOOST_TEST( r ) && BOOST_TEST_EQ( *r, 3 );
    }

    {
        result<int const> a1( 1 );

        auto r = unwrap_and_invoke( f, a1, 2 );
        BOOST_TEST( r ) && BOOST_TEST_EQ( *r, 3 );
    }

    {
        auto r = unwrap_and_invoke( f, result<int>( 1 ), 2 );
        BOOST_TEST( r ) && BOOST_TEST_EQ( *r, 3 );
    }

    //

    {
        result<int> a2( 2 );

        auto r = unwrap_and_invoke( f, 1, a2 );
        BOOST_TEST( r ) && BOOST_TEST_EQ( *r, 3 );
    }

    {
        result<int> const a2( 2 );

        auto r = unwrap_and_invoke( f, 1, a2 );
        BOOST_TEST( r ) && BOOST_TEST_EQ( *r, 3 );
    }

    {
        result<int const> a2( 2 );

        auto r = unwrap_and_invoke( f, 1, a2 );
        BOOST_TEST( r ) && BOOST_TEST_EQ( *r, 3 );
    }

    {
        auto r = unwrap_and_invoke( f, 1, result<int>( 2 ) );
        BOOST_TEST( r ) && BOOST_TEST_EQ( *r, 3 );
    }

    //

    {
        result<int> a1( 1 );
        result<int> const a2( 2 );

        auto r = unwrap_and_invoke( f, a1, a2 );
        BOOST_TEST( r ) && BOOST_TEST_EQ( *r, 3 );
    }

    {
        result<int const> a1( 1 );

        auto r = unwrap_and_invoke( f, a1, result<int>( 2 ) );
        BOOST_TEST( r ) && BOOST_TEST_EQ( *r, 3 );
    }

    //

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<int> a1( ec );

        auto r = unwrap_and_invoke( f, a1, 2 );
        BOOST_TEST( r.has_error() ) && BOOST_TEST_EQ( r.error(), ec );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<int> const a1( ec );

        auto r = unwrap_and_invoke( f, a1, 2 );
        BOOST_TEST( r.has_error() ) && BOOST_TEST_EQ( r.error(), ec );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<int const> a1( ec );

        auto r = unwrap_and_invoke( f, a1, 2 );
        BOOST_TEST( r.has_error() ) && BOOST_TEST_EQ( r.error(), ec );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        auto r = unwrap_and_invoke( f, result<int>( ec ), 2 );
        BOOST_TEST( r.has_error() ) && BOOST_TEST_EQ( r.error(), ec );
    }

    //

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<int> a2( ec );

        auto r = unwrap_and_invoke( f, 1, a2 );
        BOOST_TEST( r.has_error() ) && BOOST_TEST_EQ( r.error(), ec );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<int> const a2( ec );

        auto r = unwrap_and_invoke( f, 1, a2 );
        BOOST_TEST( r.has_error() ) && BOOST_TEST_EQ( r.error(), ec );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<int const> a2( ec );

        auto r = unwrap_and_invoke( f, 1, a2 );
        BOOST_TEST( r.has_error() ) && BOOST_TEST_EQ( r.error(), ec );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        auto r = unwrap_and_invoke( f, 1, result<int>( ec ) );
        BOOST_TEST( r.has_error() ) && BOOST_TEST_EQ( r.error(), ec );
    }

    //

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<int> a1( ec );
        result<int> const a2( 2 );

        auto r = unwrap_and_invoke( f, a1, a2 );
        BOOST_TEST( r.has_error() ) && BOOST_TEST_EQ( r.error(), ec );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<int> a1( 1 );
        result<int> const a2( ec );

        auto r = unwrap_and_invoke( f, a1, a2 );
        BOOST_TEST( r.has_error() ) && BOOST_TEST_EQ( r.error(), ec );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<int const> a1( ec );

        auto r = unwrap_and_invoke( f, a1, result<int>( 2 ) );
        BOOST_TEST( r.has_error() ) && BOOST_TEST_EQ( r.error(), ec );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<int const> a1( 1 );

        auto r = unwrap_and_invoke( f, a1, result<int>( ec ) );
        BOOST_TEST( r.has_error() ) && BOOST_TEST_EQ( r.error(), ec );
    }

    //

    {
        result<X> a1( X( 1 ) );
        result<int> const a2( 2 );

        auto r = unwrap_and_invoke( &X::f, a1, a2 );
        BOOST_TEST( r ) && BOOST_TEST_EQ( *r, 3 );
    }

    {
        result<X const> a1( X( 1 ) );

        auto r = unwrap_and_invoke( &X::f, a1, result<int>( 2 ) );
        BOOST_TEST( r ) && BOOST_TEST_EQ( *r, 3 );
    }

    //

    {
        result< std::unique_ptr<X> > a1( ::make_unique<X>( 1 ) );
        result< std::unique_ptr<X> > a2( ::make_unique<X>( 2 ) );

        auto r = unwrap_and_invoke( &X::g, std::move( a1 ), std::move( a2 ) );
        BOOST_TEST( r ) && BOOST_TEST_EQ( *r, 3 );
    }

    return boost::report_errors();
}
