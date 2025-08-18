// Copyright 2025 Christian Mazakas.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/detail/mul128.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <climits>

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

#if defined(BOOST_NO_CXX14_CONSTEXPR)
# define TEST_EQ(x1, x2) BOOST_TEST(x1 == x2)
#else
# define TEST_EQ(x1, x2) BOOST_TEST(x1 == x2); STATIC_ASSERT( x1 == x2 )
#endif

BOOST_CXX14_CONSTEXPR bool operator==( boost::hash2::detail::uint128 x, boost::hash2::detail::uint128 y ) noexcept
{
    return ( x.low == y.low ) && ( x.high == y.high );
}

int main()
{
    using boost::hash2::detail::mul128;
    using boost::hash2::detail::uint128;

    TEST_EQ( mul128( ULLONG_MAX, ULLONG_MAX ), ( uint128{ 0x0000000000000001ull, 0xfffffffffffffffeull } ) );
    TEST_EQ( mul128( 0, 0 ), ( uint128{ 0x0000000000000000ull, 0x0000000000000000ull } ) );
    TEST_EQ( mul128( 1, 2 ), ( uint128{ 0x0000000000000002ull, 0x0000000000000000ull } ) );
    TEST_EQ( mul128( 3, ULLONG_MAX ), ( uint128{ 0xfffffffffffffffdull, 0x0000000000000002ull } ) );

    return boost::report_errors();
}
