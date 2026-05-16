// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// See: https://github.com/boostorg/decimal/issues/1105

#include <boost/decimal.hpp>
#include <boost/core/lightweight_test.hpp>
#include <random>

static std::mt19937_64 rng(42);

using namespace boost::decimal;

template <typename T>
void test()
{
    std::uniform_int_distribution<int> dist(1, 1);

    const T one {dist(rng), 0};
    const T zero {0, 0};

    const T val {dist(rng), -5};
    int val_exp {};
    const auto val_sig {frexp10(val, &val_exp)};

    const auto next {nextafter(val, one)};
    int next_exp {};
    const auto next_sig {frexp10(next, &next_exp)};
    BOOST_TEST_EQ(next_exp, val_exp);
    BOOST_TEST_EQ(next_sig, val_sig + 1U);

    const auto prev {nextafter(val, zero)};
    int prev_exp {};
    const auto prev_sig {frexp10(prev, &prev_exp)};
    BOOST_TEST_EQ(prev_exp, val_exp - 1);
    BOOST_TEST_EQ(prev_sig, detail::max_significand_v<T>);

    // Max significand + 1 should be reduced
    const auto original_val {nextafter(prev, one)};
    int original_exp {};
    const auto original_sig {frexp10(original_val, &original_exp)};
    BOOST_TEST_EQ(original_exp, val_exp);
    BOOST_TEST_EQ(original_sig, val_sig);

    const auto zero_next {nextafter(zero, one)};
    BOOST_TEST_EQ(zero_next, std::numeric_limits<T>::denorm_min());
}

// Per IEEE 754 nextafter is allowed to break cohort
void test_non_preserving()
{
    const auto val = decimal32_t{"1e-100"};
    const auto two_val = decimal32_t{2, boost::decimal::detail::etiny_v<decimal32_t>};
    const auto one = decimal32_t{"1e0"};
    const auto next = nextafter(val,one);
    const auto between = decimal32_t{"11e-101"};

    BOOST_TEST_LE(val, between);
    BOOST_TEST_EQ(next, between);
    BOOST_TEST_LE(two_val, next);

    const auto nines_value = decimal32_t{"99e-101"};
    const auto next_nines_res = decimal32_t{"100e-101"};
    const auto res = nextafter(nines_value, one);
    BOOST_TEST_EQ(res, next_nines_res);

    const auto wrap_value = decimal32_t{"9999999e-107"};
    const auto next_after_wrap = nextafter(wrap_value, one);
    BOOST_TEST_GT(next_after_wrap, wrap_value);
}

int main()
{
    test<decimal32_t>();
    test<decimal64_t>();
    test<decimal128_t>();

    test_non_preserving();

    return boost::report_errors();
}
