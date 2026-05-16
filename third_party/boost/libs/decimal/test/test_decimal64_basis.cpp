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

#if BOOST_DECIMAL_ENDIAN_LITTLE_BYTE
void test_binary_constructor()
{
    decimal64_t one(0b1, -397);
    // 0 for sign
    // 00000 for combination field
    // 00000001 for exp
    // 1 for significand
    BOOST_TEST_EQ(std::bitset<64>(to_bits(one)),
                  std::bitset<64>(0b0'00000'00000001'0000000000'0000000000'0000000000'0000000000'0000000001));

    decimal64_t neg_one(-0b1, -397);
    // 1 for sign
    // 00000 for combination field
    // 00000001 for exp
    // 1 for significand
    BOOST_TEST_EQ(std::bitset<64>(to_bits(neg_one)),
                  std::bitset<64>(0b1'00000'00000001'0000000000'0000000000'0000000000'0000000000'0000000001));

    decimal64_t big_sig(0b1111111111'1111111111'1111111111'1111111111'1111111111, -397);
    // 0 for sign
    // 00000 for combination field
    // 00000001 for exp
    // 1111111111'1111111111'1111111111'1111111111'1111111111 for significand
    BOOST_TEST_EQ(std::bitset<64>(to_bits(big_sig)),
                  std::bitset<64>(0b0'00000'00000001'1111111111'1111111111'1111111111'1111111111'1111111111));


    // Construct denorm min
    decimal64_t denorm_min(0b1, -398);
    BOOST_TEST_EQ(std::bitset<64>(to_bits(denorm_min)),
                  std::bitset<64>(0b0'00000'00000000'0000000000'0000000000'0000000000'0000000000'0000000001));

    // Tests that need the combination field

    decimal64_t comb_sig(0b1'1111111111'1111111111'1111111111'1111111111'1111111111, -397);
    // 0 for sign
    // 00001 for combination field
    // 00000001 for exp
    // 1111111111'1111111111'1111111111'1111111111'1111111111 for significand
    BOOST_TEST_EQ(std::bitset<64>(to_bits(comb_sig)),
                  std::bitset<64>(0b0'00001'00000001'1111111111'1111111111'1111111111'1111111111'1111111111));

    decimal64_t comb2_sig(0b11'1111111111'1111111111'1111111111'1111111111'1111111111, -397);
    // 0 for sign
    // 00011 for combination field
    // 00000001 for exp
    // 1111111111'1111111111'1111111111'1111111111'1111111111 for significand
    BOOST_TEST_EQ(std::bitset<64>(to_bits(comb2_sig)),
                  std::bitset<64>(0b0'00011'00000001'1111111111'1111111111'1111111111'1111111111'1111111111));

    decimal64_t comb3_sig(0b111'1111111111'1111111111'1111111111'1111111111'1111111111, -397);
    // 0 for sign
    // 00011 for combination field
    // 00000001 for exp
    // 1111111111'1111111111'1111111111'1111111111'1111111111 for significand
    BOOST_TEST_EQ(std::bitset<64>(to_bits(comb3_sig)),
                  std::bitset<64>(0b0'00111'00000001'1111111111'1111111111'1111111111'1111111111'1111111111));

    decimal64_t comb4_sig(0b1000'0000000000'0000000000'0000000000'0000000000'0000000001, -397);
    // 0 for sign
    // 11000 for combination field
    // 00000001 for exp
    // 1 for significand
    BOOST_TEST_EQ(std::bitset<64>(to_bits(comb4_sig)),
                  std::bitset<64>(0b0'11000'00000001'0000000000'0000000000'0000000000'0000000000'0000000001));

    decimal64_t med_comb_exp (0b1, 0);
    // 0 for sign
    // 01000 for combination field
    // 10001110 for exp
    // 1 for significand
    BOOST_TEST_EQ(std::bitset<64>(to_bits(med_comb_exp)),
                  std::bitset<64>(0b0'01000'10001110'0000000000'0000000000'0000000000'0000000000'0000000001));

    decimal64_t big_comb_exp (0b1, 128);
    // 0 for sign
    // 10000 for combination field
    // 00001110 for exp
    // 1 for significand
    BOOST_TEST_EQ(std::bitset<64>(to_bits(big_comb_exp)),
                  std::bitset<64>(0b0'10000'00001110'0000000000'0000000000'0000000000'0000000000'0000000001));
}

#else

void test_binary_constructor()
{
    //nothing
}

#endif

void test_non_finite_values()
{
    const decimal64_t one(0b1, 0);
    const decimal64_t neg_one(0b1, 0, true);
    const decimal64_t inf_val = boost::decimal::from_bits(boost::decimal::detail::d64_inf_mask);
    const decimal64_t qnan_val = boost::decimal::from_bits(boost::decimal::detail::d64_nan_mask);
    const decimal64_t snan_val = boost::decimal::from_bits(boost::decimal::detail::d64_snan_mask);

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

void test_equality()
{
    const decimal64_t one(0b1, 0);
    const decimal64_t neg_one(0b1, 0, true);
    const decimal64_t inf_val = boost::decimal::from_bits(boost::decimal::detail::d64_inf_mask);
    const decimal64_t qnan_val = boost::decimal::from_bits(boost::decimal::detail::d64_nan_mask);
    const decimal64_t snan_val = boost::decimal::from_bits(boost::decimal::detail::d64_snan_mask);

    BOOST_TEST(!(one == neg_one));
    BOOST_TEST(one == one);
    BOOST_TEST(!(one == qnan_val));
    BOOST_TEST(!(inf_val == qnan_val));
    BOOST_TEST(!(qnan_val == qnan_val));
    BOOST_TEST(!(qnan_val == snan_val));
}

void test_inequality()
{
    const decimal64_t one(0b1, 0);
    const decimal64_t neg_one(0b1, 0, true);
    const decimal64_t inf_val = boost::decimal::from_bits(boost::decimal::detail::d64_inf_mask);
    const decimal64_t qnan_val = boost::decimal::from_bits(boost::decimal::detail::d64_nan_mask);
    const decimal64_t snan_val = boost::decimal::from_bits(boost::decimal::detail::d64_snan_mask);

    BOOST_TEST(one != neg_one);
    BOOST_TEST(!(one != one));
    BOOST_TEST(one != qnan_val);
    BOOST_TEST(inf_val != qnan_val);
    BOOST_TEST(qnan_val != qnan_val);
    BOOST_TEST(qnan_val != snan_val);
}

void test_less()
{
    const decimal64_t one(0b1, 0);
    const decimal64_t neg_one(0b1, 0, true);
    const decimal64_t inf_val = boost::decimal::from_bits(boost::decimal::detail::d64_inf_mask);
    const decimal64_t qnan_val = boost::decimal::from_bits(boost::decimal::detail::d64_nan_mask);
    const decimal64_t snan_val = boost::decimal::from_bits(boost::decimal::detail::d64_snan_mask);

    BOOST_TEST(neg_one < one);
    BOOST_TEST(!(one < one));
    BOOST_TEST(!(one < qnan_val));
    BOOST_TEST(!(inf_val < qnan_val));
    BOOST_TEST(!(qnan_val < qnan_val));
    BOOST_TEST(!(qnan_val < snan_val));
    BOOST_TEST(one < inf_val);
}

void test_le()
{
    const decimal64_t one(0b1, 0);
    const decimal64_t neg_one(0b1, 0, true);
    const decimal64_t inf_val = boost::decimal::from_bits(boost::decimal::detail::d64_inf_mask);
    const decimal64_t qnan_val = boost::decimal::from_bits(boost::decimal::detail::d64_nan_mask);
    const decimal64_t snan_val = boost::decimal::from_bits(boost::decimal::detail::d64_snan_mask);

    BOOST_TEST(neg_one <= one);
    BOOST_TEST(one <= one);
    BOOST_TEST(!(one <= qnan_val));
    BOOST_TEST(!(inf_val <= qnan_val));
    BOOST_TEST(!(qnan_val <= qnan_val));
    BOOST_TEST(!(qnan_val <= snan_val));
}

void test_greater()
{
    const decimal64_t one(0b1, 0);
    const decimal64_t neg_one(0b1, 0, true);
    const decimal64_t inf_val = boost::decimal::from_bits(boost::decimal::detail::d64_inf_mask);
    const decimal64_t qnan_val = boost::decimal::from_bits(boost::decimal::detail::d64_nan_mask);
    const decimal64_t snan_val = boost::decimal::from_bits(boost::decimal::detail::d64_snan_mask);

    BOOST_TEST(!(neg_one > one));
    BOOST_TEST(one > neg_one);
    BOOST_TEST(!(one > one));
    BOOST_TEST(!(one > qnan_val));
    BOOST_TEST(!(inf_val > qnan_val));
    BOOST_TEST(!(qnan_val > qnan_val));
    BOOST_TEST(!(qnan_val > snan_val));
}

void test_ge()
{
    const decimal64_t one(0b1, 0);
    const decimal64_t neg_one(0b1, 0, true);
    const decimal64_t inf_val = boost::decimal::from_bits(boost::decimal::detail::d64_inf_mask);
    const decimal64_t qnan_val = boost::decimal::from_bits(boost::decimal::detail::d64_nan_mask);
    const decimal64_t snan_val = boost::decimal::from_bits(boost::decimal::detail::d64_snan_mask);

    BOOST_TEST(!(neg_one >= one));
    BOOST_TEST(one >= neg_one);
    BOOST_TEST(one >= one);
    BOOST_TEST(!(one >= qnan_val));
    BOOST_TEST(!(inf_val >= qnan_val));
    BOOST_TEST(!(qnan_val >= qnan_val));
    BOOST_TEST(!(qnan_val >= snan_val));
}

int main()
{
    test_binary_constructor();

    test_non_finite_values();

    test_equality();
    test_inequality();
    test_less();
    test_le();
    test_greater();
    test_ge();

    return boost::report_errors();
}
