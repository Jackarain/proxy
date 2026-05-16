// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// See: https://github.com/boostorg/decimal/issues/1294

#include <boost/decimal.hpp>
#include <boost/core/lightweight_test.hpp>

using namespace boost::decimal;

template <typename T>
void test_round_down()
{
    const T val {"0.499"};
    constexpr T ref_1 {1};
    constexpr T ref_0 {0};

    BOOST_TEST_EQ(ceil(val), ref_1);
    BOOST_TEST_EQ(floor(val), ref_0);
    BOOST_TEST_EQ(trunc(val), ref_0);
    BOOST_TEST_EQ(round(val), ref_0);
    BOOST_TEST_EQ(lround(val), 0L);
    BOOST_TEST_EQ(nearbyint(val), 0);
    BOOST_TEST_EQ(lrint(val), 0L);
    BOOST_TEST_EQ(llrint(val), 0LL);
}

template <typename T>
void test()
{
    const T val {"0.999"};
    constexpr T ref_1 {1};
    constexpr T ref_0 {0};

    BOOST_TEST_EQ(ceil(val), ref_1);
    BOOST_TEST_EQ(floor(val), ref_0);
    BOOST_TEST_EQ(trunc(val), ref_0);
    BOOST_TEST_EQ(round(val), ref_1);
    BOOST_TEST_EQ(lround(val), 1L);
    BOOST_TEST_EQ(nearbyint(val), 1);
    BOOST_TEST_EQ(lrint(val), 1L);
    BOOST_TEST_EQ(llrint(val), 1LL);

    test_round_down<T>();
}

int main()
{
    test<decimal32_t>();
    test<decimal64_t>();
    test<decimal128_t>();

    test<decimal_fast32_t>();
    test<decimal_fast64_t>();
    test<decimal_fast128_t>();

    return boost::report_errors();
}
