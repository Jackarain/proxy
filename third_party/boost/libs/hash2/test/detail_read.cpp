// Copyright 2024 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/detail/read.hpp>
#include <boost/core/lightweight_test.hpp>

int main()
{
    using namespace boost::hash2::detail;

    {
        unsigned char w[ 4 ] = { 0x01, 0x02, 0x03, 0x04 };

        BOOST_TEST_EQ( read32be( w ), 0x01020304 );
        BOOST_TEST_EQ( read32le( w ), 0x04030201 );
    }

    {
        unsigned char w[ 8 ] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };

        BOOST_TEST_EQ( read64be( w ), 0x0102030405060708 );
        BOOST_TEST_EQ( read64le( w ), 0x0807060504030201 );
    }

    return boost::report_errors();
}
