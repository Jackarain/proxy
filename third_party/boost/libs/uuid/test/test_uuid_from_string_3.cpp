// Copyright 2025, 2026 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/core/lightweight_test.hpp>
#include <string>

using namespace boost::uuids;

template<class Ch> void test( Ch const* str, std::string expected )
{
    try
    {
        uuid_from_string( str );
        BOOST_ERROR( "uuid_from_string failed to throw" );
    }
    catch( std::exception const& x )
    {
        BOOST_TEST_EQ( expected, std::string( x.what() ) );
    }
}

int main()
{
    test(  "", "Invalid UUID string at position 0: unexpected end of input" );
    test(  L"0", "Invalid UUID string at position 1: unexpected end of input" );
    test(  u"01", "Invalid UUID string at position 2: unexpected end of input" );
    test(  U"01234567-89aB-cDeF-0123-456789AbCd", "Invalid UUID string at position 34: unexpected end of input" );
    test( u8"01234567-89aB-cDeF-0123-456789AbCdE", "Invalid UUID string at position 35: unexpected end of input" );

    test(   "@", "Invalid UUID string at position 0: hex digit expected" );
    test(  L"G", "Invalid UUID string at position 0: hex digit expected" );
    test(  u"g", "Invalid UUID string at position 0: hex digit expected" );
    test(  U"-", "Invalid UUID string at position 0: hex digit expected" );
    test( u8"abcdefgh-0000-0000-0000-000000000000", "Invalid UUID string at position 6: hex digit expected" );

    test(   "0000000000000-0000-0000-000000000000", "Invalid UUID string at position 8: dash expected" );
    test(  L"00000000-000000000-0000-000000000000", "Invalid UUID string at position 13: dash expected" );
    test(  u"00000000-0000-000000000-000000000000", "Invalid UUID string at position 18: dash expected" );
    test(  U"00000000-0000-0000-00000000000000000", "Invalid UUID string at position 23: dash expected" );

    test(   "00000000-0000-0000-0000-000000000000-0000", "Invalid UUID string at position 36: unexpected extra input" );
    test(  L"00010203-0405-0607-0809-0a0b0c0d0e0f1011", "Invalid UUID string at position 36: unexpected extra input" );
    test(  u"12345678-90AB-CDEF-1234-567890abcdefghij", "Invalid UUID string at position 36: unexpected extra input" );

    return boost::report_errors();
}
