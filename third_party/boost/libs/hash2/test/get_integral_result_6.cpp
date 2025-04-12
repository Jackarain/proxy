// Copyright 2017, 2018, 2024, 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/get_integral_result.hpp>
#include <boost/hash2/fnv1a.hpp>
#include <boost/core/lightweight_test.hpp>
#include <cstdint>

using boost::hash2::fnv1a_64;
using boost::hash2::get_integral_result;

template<class R> struct fnv1a_x: private fnv1a_64
{
    using result_type = R;

    result_type result()
    {
        return get_integral_result<R>( static_cast<fnv1a_64&>( *this ) );
    }
};

int main()
{
    // 1 -> 2
    {
        fnv1a_x<std::uint8_t> h1;

        std::uint16_t r1 = get_integral_result<std::int16_t>( h1 );

        fnv1a_x<std::uint8_t> h2;

        std::uint16_t r2 = 0;

        r2 += static_cast<std::uint16_t>( h2.result() ) << 0;
        r2 += static_cast<std::uint16_t>( h2.result() ) << 8;

        BOOST_TEST_EQ( static_cast<std::uint16_t>( r1 ), r2 );
    }

    // 1 -> 4
    {
        fnv1a_x<std::uint8_t> h1;

        std::uint32_t r1 = get_integral_result<std::int32_t>( h1 );

        fnv1a_x<std::uint8_t> h2;

        std::uint32_t r2 = 0;

        r2 += static_cast<std::uint32_t>( h2.result() ) <<  0;
        r2 += static_cast<std::uint32_t>( h2.result() ) <<  8;
        r2 += static_cast<std::uint32_t>( h2.result() ) << 16;
        r2 += static_cast<std::uint32_t>( h2.result() ) << 24;

        BOOST_TEST_EQ( static_cast<std::uint32_t>( r1 ), r2 );
    }

    // 1 -> 8
    {
        fnv1a_x<std::uint8_t> h1;

        std::uint64_t r1 = get_integral_result<std::int64_t>( h1 );

        fnv1a_x<std::uint8_t> h2;

        std::uint64_t r2 = 0;

        r2 += static_cast<std::uint64_t>( h2.result() ) <<  0;
        r2 += static_cast<std::uint64_t>( h2.result() ) <<  8;
        r2 += static_cast<std::uint64_t>( h2.result() ) << 16;
        r2 += static_cast<std::uint64_t>( h2.result() ) << 24;
        r2 += static_cast<std::uint64_t>( h2.result() ) << 32;
        r2 += static_cast<std::uint64_t>( h2.result() ) << 40;
        r2 += static_cast<std::uint64_t>( h2.result() ) << 48;
        r2 += static_cast<std::uint64_t>( h2.result() ) << 56;

        BOOST_TEST_EQ( static_cast<std::uint64_t>( r1 ), r2 );
    }

    // 2 -> 4
    {
        fnv1a_x<std::uint16_t> h1;

        std::uint32_t r1 = get_integral_result<std::int32_t>( h1 );

        fnv1a_x<std::uint16_t> h2;

        std::uint32_t r2 = 0;

        r2 += static_cast<std::uint32_t>( h2.result() ) <<  0;
        r2 += static_cast<std::uint32_t>( h2.result() ) << 16;

        BOOST_TEST_EQ( static_cast<std::uint32_t>( r1 ), r2 );
    }

    // 2 -> 8
    {
        fnv1a_x<std::uint16_t> h1;

        std::uint64_t r1 = get_integral_result<std::int64_t>( h1 );

        fnv1a_x<std::uint16_t> h2;

        std::uint64_t r2 = 0;

        r2 += static_cast<std::uint64_t>( h2.result() ) <<  0;
        r2 += static_cast<std::uint64_t>( h2.result() ) << 16;
        r2 += static_cast<std::uint64_t>( h2.result() ) << 32;
        r2 += static_cast<std::uint64_t>( h2.result() ) << 48;

        BOOST_TEST_EQ( static_cast<std::uint64_t>( r1 ), r2 );
    }

    // 4 -> 8
    {
        fnv1a_x<std::uint32_t> h1;

        std::uint64_t r1 = get_integral_result<std::int64_t>( h1 );

        fnv1a_x<std::uint32_t> h2;

        std::uint64_t r2 = 0;

        r2 += static_cast<std::uint64_t>( h2.result() ) <<  0;
        r2 += static_cast<std::uint64_t>( h2.result() ) << 32;

        BOOST_TEST_EQ( static_cast<std::uint64_t>( r1 ), r2 );
    }

    return boost::report_errors();
}
