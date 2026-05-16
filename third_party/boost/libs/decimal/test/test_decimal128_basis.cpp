// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include "mini_to_chars.hpp"

#include <boost/decimal.hpp>
#include <boost/core/lightweight_test.hpp>
#include <bitset>
#include <limits>
#include <random>
#include <cmath>
#include <cerrno>

using namespace boost::decimal;

void test_non_finite_values()
{
    const decimal128_t one(0b1, 0);
    const decimal128_t neg_one(0b1, 0, true);
    const decimal128_t inf_val = boost::decimal::from_bits(boost::decimal::detail::d128_inf_mask);
    const decimal128_t qnan_val = boost::decimal::from_bits(boost::decimal::detail::d128_nan_mask);
    const decimal128_t snan_val = boost::decimal::from_bits(boost::decimal::detail::d128_snan_mask);

    BOOST_TEST(!signbit(one));
    BOOST_TEST(signbit(neg_one));
    BOOST_TEST(!signbit(inf_val));
    BOOST_TEST(!signbit(qnan_val));
    BOOST_TEST(!signbit(snan_val));

    BOOST_TEST(!isinf(one));
    BOOST_TEST(!isinf(neg_one));
    BOOST_TEST(isinf(inf_val));
    BOOST_TEST(!isinf(qnan_val));
    BOOST_TEST(!isinf(snan_val));

    BOOST_TEST(!isnan(one));
    BOOST_TEST(!isnan(neg_one));
    BOOST_TEST(!isnan(inf_val));
    BOOST_TEST(isnan(qnan_val));
    BOOST_TEST(isnan(snan_val));

    BOOST_TEST(!issignaling(one));
    BOOST_TEST(!issignaling(neg_one));
    BOOST_TEST(!issignaling(inf_val));
    BOOST_TEST(!issignaling(qnan_val));
    BOOST_TEST(issignaling(snan_val));

    BOOST_TEST(isnormal(one));
    BOOST_TEST(isnormal(neg_one));
    BOOST_TEST(!isnormal(inf_val));
    BOOST_TEST(!isnormal(qnan_val));
    BOOST_TEST(!isnormal(snan_val));
}

void test_float_constructor()
{
    #if BOOST_DECIMAL_LDBL_BITS > 64
    const decimal128_t val(8.45150093187788174e+4929L);
    const decimal128_t computed_val {845150093187788174, 4912};
    BOOST_TEST_EQ(val, computed_val);

    const decimal128_t val2(6.277323608720053612e+4909L);
    const decimal128_t computed_val2 {6277323608720053612, 4891};
    BOOST_TEST_EQ(val2, computed_val2);
    #endif
}

int main()
{
    test_non_finite_values();

    test_float_constructor();

    return boost::report_errors();
}
