// Copyright 2025 Christian Mazakas.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/detail/byteswap.hpp>
#include <boost/core/lightweight_test.hpp>

// shamelessly steal pdimov's tests from Core

int main()
{
    using boost::hash2::detail::byteswap;

    BOOST_TEST_EQ( byteswap( std::uint32_t{ 0xf1e2d3c4u } ), 0xc4d3e2f1u );
    BOOST_TEST_EQ( byteswap( std::uint64_t{ 0xf1e2d3c4u } << 32 | 0xb5a69788u ), ( std::uint64_t{ 0x8897a6b5u } << 32 | 0xc4d3e2f1u) );

    return boost::report_errors();
}
