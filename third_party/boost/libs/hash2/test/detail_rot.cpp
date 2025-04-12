// Copyright 2024 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/detail/rot.hpp>
#include <boost/core/lightweight_test.hpp>

int main()
{
    using namespace boost::hash2::detail;

    {
        std::uint32_t v = 0x9e3779b9;

        for( int k = 1; k < 32; ++k )
        {
            BOOST_TEST_EQ( rotl( v, k ), ( v << k ) + ( v >> ( 32 - k ) ) );
            BOOST_TEST_EQ( rotr( v, k ), ( v >> k ) + ( v << ( 32 - k ) ) );
        }
    }

    {
        std::uint64_t v = 0x9e3779b97f4a7c15;

        for( int k = 1; k < 64; ++k )
        {
            BOOST_TEST_EQ( rotl( v, k ), ( v << k ) + ( v >> ( 64 - k ) ) );
            BOOST_TEST_EQ( rotr( v, k ), ( v >> k ) + ( v << ( 64 - k ) ) );
        }
    }

    return boost::report_errors();
}
