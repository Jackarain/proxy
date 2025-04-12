//  Copyright (C) 2011-2013 Tim Blechmann
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/lockfree/spsc_value.hpp>

#define BOOST_TEST_MAIN
#ifdef BOOST_LOCKFREE_INCLUDE_TESTS
#    include <boost/test/included/unit_test.hpp>
#else
#    include <boost/test/unit_test.hpp>
#endif

BOOST_AUTO_TEST_CASE( spsc_value_test )
{
    boost::lockfree::spsc_value< uint64_t > v;

    auto validate_value = [ & ]( uint64_t expected ) {
        uint64_t out {};
        BOOST_TEST_REQUIRE( v.read( out ) == true );
        BOOST_TEST_REQUIRE( out == expected );
    };

    auto validate_no_pending_update = [ & ] {
        uint64_t out {};
        BOOST_TEST_REQUIRE( v.read( out ) == false );
    };

    validate_no_pending_update();

    v.write( 1 );
    validate_value( 1 );

    v.write( 2 );
    validate_value( 2 );
    v.write( 3 );
    validate_value( 3 );
    v.write( 4 );
    validate_value( 4 );
    validate_no_pending_update();
    validate_no_pending_update();
    validate_no_pending_update();

    v.write( 5 );
    v.write( 6 );
    v.write( 7 );
    validate_value( 7 );
}

BOOST_AUTO_TEST_CASE( spsc_value_test_allow_duplicate_reads )
{
    boost::lockfree::spsc_value< uint64_t, boost::lockfree::allow_multiple_reads< true > > v { 0xff };

    auto validate_value = [ & ]( uint64_t expected ) {
        uint64_t out {};
        BOOST_TEST_REQUIRE( v.read( out ) == true );
        BOOST_TEST_REQUIRE( out == expected );
    };

    validate_value( 0xff );

    v.write( 1 );
    validate_value( 1 );

    v.write( 2 );
    validate_value( 2 );
    v.write( 3 );
    validate_value( 3 );
    v.write( 4 );
    validate_value( 4 );
    validate_value( 4 );
    validate_value( 4 );
    validate_value( 4 );

    v.write( 5 );
    v.write( 6 );
    v.write( 7 );
    validate_value( 7 );
}


BOOST_AUTO_TEST_CASE( spsc_value_test_move_only_type )
{
    auto make_t = [ & ]( uint64_t val ) {
        return std::make_unique< uint64_t >( val );
    };

    boost::lockfree::spsc_value< std::unique_ptr< uint64_t > > v;

    auto validate_value = [ & ]( uint64_t expected ) {
        std::unique_ptr< uint64_t > out;
        BOOST_TEST_REQUIRE( v.read( out ) == true );
        BOOST_TEST_REQUIRE( *out == expected );
    };

    auto t1 = make_t( 1 );
    auto t2 = make_t( 2 );
    auto t3 = make_t( 3 );

    v.write( std::move( t1 ) );
    validate_value( 1 );

    v.write( std::move( t2 ) );
    BOOST_TEST_REQUIRE( v.consume( []( const std::unique_ptr< uint64_t >& out ) {
        BOOST_TEST_REQUIRE( *out == uint64_t( 2 ) );
    } ) );

    v.write( std::move( t3 ) );
    BOOST_TEST_REQUIRE( v.consume( []( std::unique_ptr< uint64_t > out ) {
        BOOST_TEST_REQUIRE( *out == uint64_t( 3 ) );
    } ) );

    BOOST_TEST_REQUIRE( v.consume( []( std::unique_ptr< uint64_t > out ) {} ) == false );

    std::unique_ptr< uint64_t > out;
    BOOST_TEST_REQUIRE( v.read( out ) == false );
}
