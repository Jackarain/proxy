// Copyright 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/detail/write.hpp>
#include <boost/core/lightweight_test.hpp>
#include <array>

int main()
{
    using namespace boost::hash2;

    {
        unsigned char w[ 1 ] = {};

        detail::write( static_cast<std::int8_t>( 0x01 ), endian::little, w );

        BOOST_TEST_EQ( w[ 0 ], 0x01 );
    }

    {
        unsigned char w[ 1 ] = {};

        detail::write( static_cast<std::int8_t>( 0x01 ), endian::big, w );

        BOOST_TEST_EQ( w[ 0 ], 0x01 );
    }

    {
        unsigned char w[ 2 ] = {};

        detail::write( static_cast<std::int16_t>( 0x0102 ), endian::little, w );

        BOOST_TEST_EQ( w[ 0 ], 0x02 );
        BOOST_TEST_EQ( w[ 1 ], 0x01 );
    }

    {
        unsigned char w[ 2 ] = {};

        detail::write( static_cast<std::int16_t>( 0x0102 ), endian::big, w );

        BOOST_TEST_EQ( w[ 0 ], 0x01 );
        BOOST_TEST_EQ( w[ 1 ], 0x02 );
    }

    {
        unsigned char w[ 4 ] = {};

        detail::write( static_cast<std::int32_t>( 0x01020304 ), endian::little, w );

        BOOST_TEST_EQ( w[ 0 ], 0x04 );
        BOOST_TEST_EQ( w[ 1 ], 0x03 );
        BOOST_TEST_EQ( w[ 2 ], 0x02 );
        BOOST_TEST_EQ( w[ 3 ], 0x01 );
    }

    {
        unsigned char w[ 4 ] = {};

        detail::write( static_cast<std::int32_t>( 0x01020304 ), endian::big, w );

        BOOST_TEST_EQ( w[ 0 ], 0x01 );
        BOOST_TEST_EQ( w[ 1 ], 0x02 );
        BOOST_TEST_EQ( w[ 2 ], 0x03 );
        BOOST_TEST_EQ( w[ 3 ], 0x04 );
    }

    {
        unsigned char w[ 8 ] = {};

        detail::write( static_cast<std::int64_t>( 0x0102030405060708 ), endian::little, w );

        BOOST_TEST_EQ( w[ 0 ], 0x08 );
        BOOST_TEST_EQ( w[ 1 ], 0x07 );
        BOOST_TEST_EQ( w[ 2 ], 0x06 );
        BOOST_TEST_EQ( w[ 3 ], 0x05 );
        BOOST_TEST_EQ( w[ 4 ], 0x04 );
        BOOST_TEST_EQ( w[ 5 ], 0x03 );
        BOOST_TEST_EQ( w[ 6 ], 0x02 );
        BOOST_TEST_EQ( w[ 7 ], 0x01 );
    }

    {
        unsigned char w[ 8 ] = {};

        detail::write( static_cast<std::int64_t>( 0x0102030405060708 ), endian::big, w );

        BOOST_TEST_EQ( w[ 0 ], 0x01 );
        BOOST_TEST_EQ( w[ 1 ], 0x02 );
        BOOST_TEST_EQ( w[ 2 ], 0x03 );
        BOOST_TEST_EQ( w[ 3 ], 0x04 );
        BOOST_TEST_EQ( w[ 4 ], 0x05 );
        BOOST_TEST_EQ( w[ 5 ], 0x06 );
        BOOST_TEST_EQ( w[ 6 ], 0x07 );
        BOOST_TEST_EQ( w[ 7 ], 0x08 );
    }

    return boost::report_errors();
}
