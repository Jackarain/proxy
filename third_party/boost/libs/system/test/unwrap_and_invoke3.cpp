// Copyright 2026 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/system/unwrap_and_invoke.hpp>
#include <boost/system/result.hpp>
#include <boost/core/lightweight_test.hpp>

using namespace boost::system;

int g_x;

void f( int x )
{
    g_x += x;
}

int main()
{
    {
        g_x = 0;

        result<int> a1( 1 );
        result<void> r = unwrap_and_invoke( f, a1 );

        BOOST_TEST( r );
        BOOST_TEST_EQ( g_x, 1 );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<int> a1( ec );
        result<void> r = unwrap_and_invoke( f, a1 );

        BOOST_TEST( r.has_error() ) && BOOST_TEST_EQ( r.error(), ec );
    }

    return boost::report_errors();
}
