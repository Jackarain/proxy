// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// See: https://github.com/boostorg/decimal/issues/1112

#include <boost/decimal.hpp>
#include <boost/core/lightweight_test.hpp>
#include <random>

static std::mt19937_64 rng(42);
std::uniform_int_distribution<> dist(-1, -1);

using namespace boost::decimal;

template <typename T>
void reproducer()
{
    const auto dec_res {static_cast<int>(nearbyint(T{2325, -1}))};
    const auto dbl_res {static_cast<int>(std::nearbyint(232.5))};
    BOOST_TEST_EQ(dec_res, dbl_res);
}

template <typename T>
void test_rounding_up()
{
    const auto mode {boost::decimal::fesetround(rounding_mode::fe_dec_upward)};
    if (mode != rounding_mode::fe_dec_default)
    {
        const T value {2325, dist(rng)}; // The result will be wrong if computed at compile time
        const auto dec_res {static_cast<int>(nearbyint(value))};
        BOOST_TEST_EQ(dec_res, 233);
    }
}

template <typename T>
void test_rounding_down()
{
    const auto mode {boost::decimal::fesetround(rounding_mode::fe_dec_downward)};
    if (mode != rounding_mode::fe_dec_default)
    {
        const T value {2325, dist(rng)}; // The result will be wrong if computed at compile time
        const auto dec_res {static_cast<int>(nearbyint(value))};
        BOOST_TEST_EQ(dec_res, 232);
    }
}

int main()
{
    reproducer<decimal32_t>();
    reproducer<decimal64_t>();
    reproducer<decimal128_t>();

    test_rounding_up<decimal32_t>();
    test_rounding_up<decimal64_t>();
    test_rounding_up<decimal128_t>();

    test_rounding_down<decimal32_t>();
    test_rounding_down<decimal64_t>();
    test_rounding_down<decimal128_t>();

    return boost::report_errors();
}
