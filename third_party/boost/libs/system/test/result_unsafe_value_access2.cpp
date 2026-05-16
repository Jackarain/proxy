// Copyright 2017, 2021, 2022, 2026 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#define BOOST_ENABLE_ASSERT_HANDLER

#include <boost/system/result.hpp>
#include <boost/core/lightweight_test.hpp>

struct assertion_failure
{
};

namespace boost
{

void assertion_failed( char const* /*expr*/, char const* /*function*/, char const* /*file*/, long /*line*/ )
{
    throw assertion_failure();
}

} // namespace boost

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
        auto ec = make_error_code( errc::invalid_argument );
        result<int> r( ec );

        BOOST_TEST( !r.has_value() );
        BOOST_TEST_THROWS( r.unsafe_value(), assertion_failure );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );
        result<int> const r( ec );

        BOOST_TEST( !r.has_value() );
        BOOST_TEST_THROWS( r.unsafe_value(), assertion_failure );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        BOOST_TEST( !result<int>( ec ).has_value() );
        BOOST_TEST_THROWS( result<int>( ec ).unsafe_value(), assertion_failure );
    }

    //

    {
        auto ec = make_error_code( errc::invalid_argument );
        result<X> r( ec );

        BOOST_TEST( !r.has_value() );
        BOOST_TEST_THROWS( r.unsafe_value(), assertion_failure );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );
        result<X> const r( ec );

        BOOST_TEST( !r.has_value() );
        BOOST_TEST_THROWS( r.unsafe_value(), assertion_failure );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        BOOST_TEST(( !result<X>( ec ).has_value() ));
        BOOST_TEST_THROWS( (result<X>( ec ).unsafe_value()), assertion_failure );
    }

    //

    {
        auto ec = make_error_code( errc::invalid_argument );
        result<void> r( ec );

        BOOST_TEST( !r.has_value() );
        BOOST_TEST_THROWS( r.unsafe_value(), assertion_failure );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );
        result<void> const r( ec );

        BOOST_TEST( !r.has_value() );
        BOOST_TEST_THROWS( r.unsafe_value(), assertion_failure );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        BOOST_TEST( !result<void>( ec ).has_value() );
        BOOST_TEST_THROWS( result<void>( ec ).unsafe_value(), assertion_failure );
    }

    //

    {
        auto ec = make_error_code( errc::invalid_argument );
        result<int&> r( ec );

        BOOST_TEST( !r.has_value() );
        BOOST_TEST_THROWS( r.unsafe_value(), assertion_failure );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );
        result<int&> const r( ec );

        BOOST_TEST( !r.has_value() );
        BOOST_TEST_THROWS( r.unsafe_value(), assertion_failure );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        BOOST_TEST( !result<int&>( ec ).has_value() );
        BOOST_TEST_THROWS( result<int&>( ec ).unsafe_value(), assertion_failure );
    }

    //

    return boost::report_errors();
}
