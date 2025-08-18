// Copyright 2025 Christian Mazakas.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/detail/mul128.hpp>
#include <boost/core/lightweight_test.hpp>
#include <climits>

void test( std::uint64_t x, std::uint64_t y, boost::hash2::detail::uint128 e )
{
    auto r = boost::hash2::detail::mul128( x, y );
    BOOST_TEST_EQ( r.low, e.low );
    BOOST_TEST_EQ( r.high, e.high );
}

int main()
{
    test( ULLONG_MAX, ULLONG_MAX, { 0x0000000000000001ull, 0xfffffffffffffffeull } );
    test( 0, 0, { 0x0000000000000000ull, 0x0000000000000000ull } );
    test( 1, 2, { 0x0000000000000002ull, 0x0000000000000000ull } );
    test( 3, ULLONG_MAX, { 0xfffffffffffffffdull, 0x0000000000000002ull });

    return boost::report_errors();
}
