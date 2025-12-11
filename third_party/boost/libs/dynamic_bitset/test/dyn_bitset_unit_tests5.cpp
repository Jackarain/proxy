// -----------------------------------------------------------
//              Copyright (c) 2001 Jeremy Siek
//         Copyright (c) 2003-2006, 2025 Gennaro Prota
//
// Copyright (c) 2015 Seth Heeren
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// -----------------------------------------------------------

#include "bitset_test.hpp"
#include "boost/archive/binary_iarchive.hpp"
#include "boost/archive/binary_oarchive.hpp"
#include "boost/archive/xml_iarchive.hpp"
#include "boost/archive/xml_oarchive.hpp"
#include "boost/config.hpp"
#include "boost/dynamic_bitset/serialization.hpp"
#include "boost/serialization/vector.hpp"

#if ! defined( BOOST_NO_STRINGSTREAM )
#    include <sstream>
#endif

#if defined BOOST_NO_STD_WSTRING
#    define BOOST_DYNAMIC_BITSET_NO_WCHAR_T_TESTS
#endif

namespace {
template< typename Block >
struct SerializableType
{
    boost::dynamic_bitset< Block > x;

private:
    friend class boost::serialization::access;
    template< class Archive >
    void
    serialize( Archive & ar, unsigned int )
    {
        ar & BOOST_SERIALIZATION_NVP( x );
    }
};

template< typename Block, typename IArchive, typename OArchive >
void
test_serialization()
{
    SerializableType< Block > a;

    for ( int i = 0; i < 128; ++i )
        a.x.resize( 11 * i, i % 2 );

#if ! defined( BOOST_NO_STRINGSTREAM )
    std::stringstream ss;

    // test serialization
    {
        OArchive oa( ss );
        oa << BOOST_SERIALIZATION_NVP( a );
    }

    // test de-serialization
    {
        IArchive                  ia( ss );
        SerializableType< Block > b;
        ia >> BOOST_SERIALIZATION_NVP( b );

        BOOST_TEST( a.x == b.x );
    }
#else
#    error "TODO implement file-based test path?"
#endif
}

template< typename Block >
void
test_binary_archive()
{
    test_serialization< Block, boost::archive::binary_iarchive, boost::archive::binary_oarchive >();
}

template< typename Block >
void
test_xml_archive()
{
    test_serialization< Block, boost::archive::xml_iarchive, boost::archive::xml_oarchive >();
}
}

template< typename Block >
void
run_test_cases()
{
    test_binary_archive< Block >();
    test_xml_archive< Block >();
}

int
main()
{
    run_test_cases< unsigned char >();
    run_test_cases< unsigned short >();
    run_test_cases< unsigned int >();
    run_test_cases< unsigned long >();
    run_test_cases< unsigned long long >();

    return boost::report_errors();
}
