// Copyright 2017, 2018 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifdef _MSC_VER
# pragma warning(disable: 4127) // conditional expression is constant
#endif

#include <boost/hash2/endian.hpp>
#include <boost/core/lightweight_test.hpp>
#include <cstdint>
#include <cstring>

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

int main()
{
    using namespace boost::hash2;

    BOOST_TEST_NE( static_cast<int>( endian::little ), static_cast<int>( endian::big ) );

    unsigned char const v[ 4 ] = { 0x01, 0x02, 0x03, 0x04 };

    std::uint32_t w;

    STATIC_ASSERT( sizeof( w ) == 4 );

    std::memcpy( &w, v, 4 );

    if( endian::native == endian::little )
    {
        BOOST_TEST_EQ( w, 0x04030201 );
    }
    else if( endian::native == endian::big )
    {
        BOOST_TEST_EQ( w, 0x01020304 );
    }
    else
    {
        BOOST_TEST_NE( w, 0x04030201 );
        BOOST_TEST_NE( w, 0x01020304 );
    }

    return boost::report_errors();
}
