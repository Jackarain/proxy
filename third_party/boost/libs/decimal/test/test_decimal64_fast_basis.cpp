// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include "mini_to_chars.hpp"

#include <boost/decimal.hpp>
#include <bitset>
#include <limits>
#include <random>
#include <cmath>
#include <cerrno>

#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wold-style-cast"
#  pragma clang diagnostic ignored "-Wundef"
#  pragma clang diagnostic ignored "-Wconversion"
#  pragma clang diagnostic ignored "-Wsign-conversion"
#  pragma clang diagnostic ignored "-Wfloat-equal"
#elif defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wold-style-cast"
#  pragma GCC diagnostic ignored "-Wundef"
#  pragma GCC diagnostic ignored "-Wconversion"
#  pragma GCC diagnostic ignored "-Wsign-conversion"
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#endif

#include <boost/core/lightweight_test.hpp>

using namespace boost::decimal;

void test_comp()
{
    constexpr decimal_fast64_t small(1, -50);

    BOOST_TEST(small == small);

    constexpr decimal_fast64_t sig(123456, -50);
    BOOST_TEST(sig != small);

    BOOST_TEST(small < sig);
    BOOST_TEST(small <= sig);
    BOOST_TEST(small <= small);
    BOOST_TEST(sig > small);
    BOOST_TEST(sig >= small);

    decimal_fast64_t zero {0, 0};
    decimal_fast64_t one {1, 0};
    decimal_fast64_t half {5, -1};
    BOOST_TEST(zero < one);
    BOOST_TEST(zero < half);
    BOOST_TEST(one > zero);
    BOOST_TEST(half > zero);
    BOOST_TEST(zero > -one);
    BOOST_TEST(half > -one);
    BOOST_TEST(-one < zero);
    BOOST_TEST(-one < half);

    // Test cohorts
    BOOST_TEST(small == decimal_fast64_t(10, -51));
    BOOST_TEST(small == decimal_fast64_t(100, -52));
    BOOST_TEST(small == decimal_fast64_t(1000, -53));
    BOOST_TEST(small == decimal_fast64_t(10000, -54));
    BOOST_TEST(small == decimal_fast64_t(100000, -55));
    BOOST_TEST(small == decimal_fast64_t(1000000, -56));

    // Test non-finite comp
    BOOST_TEST(small < std::numeric_limits<decimal_fast64_t>::infinity());
    BOOST_TEST(small > -std::numeric_limits<decimal_fast64_t>::infinity());
    BOOST_TEST(!(small == std::numeric_limits<decimal_fast64_t>::infinity()));
    BOOST_TEST(small != std::numeric_limits<decimal_fast64_t>::infinity());

    BOOST_TEST(!(small < std::numeric_limits<decimal_fast64_t>::signaling_NaN()));
    BOOST_TEST(!(small < std::numeric_limits<decimal_fast64_t>::quiet_NaN()));
    BOOST_TEST(small != std::numeric_limits<decimal_fast64_t>::quiet_NaN());
    BOOST_TEST(std::numeric_limits<decimal_fast64_t>::quiet_NaN() != std::numeric_limits<decimal_fast64_t>::quiet_NaN());

    BOOST_TEST(small <= std::numeric_limits<decimal_fast64_t>::infinity());
    BOOST_TEST(small >= -std::numeric_limits<decimal_fast64_t>::infinity());
    BOOST_TEST(!(small <= std::numeric_limits<decimal_fast64_t>::signaling_NaN()));
    BOOST_TEST(!(small <= std::numeric_limits<decimal_fast64_t>::quiet_NaN()));
}

void test_non_finite_values()
{
    constexpr decimal_fast64_t one(0b1, 0);

    BOOST_TEST(std::numeric_limits<decimal_fast64_t>::has_infinity);
    BOOST_TEST(isinf(std::numeric_limits<decimal_fast64_t>::infinity()));
    BOOST_TEST(isinf(-std::numeric_limits<decimal_fast64_t>::infinity()));
    BOOST_TEST(!isinf(one));
    BOOST_TEST(!isinf(std::numeric_limits<decimal_fast64_t>::quiet_NaN()));
    BOOST_TEST(!isinf(std::numeric_limits<decimal_fast64_t>::signaling_NaN()));

    BOOST_TEST(std::numeric_limits<decimal_fast64_t>::has_quiet_NaN);
    BOOST_TEST(std::numeric_limits<decimal_fast64_t>::has_signaling_NaN);
    BOOST_TEST(isnan(std::numeric_limits<decimal_fast64_t>::quiet_NaN()));
    BOOST_TEST(isnan(std::numeric_limits<decimal_fast64_t>::signaling_NaN()));
    BOOST_TEST(!isnan(one));
    BOOST_TEST(!isnan(std::numeric_limits<decimal_fast64_t>::infinity()));
    BOOST_TEST(!isnan(-std::numeric_limits<decimal_fast64_t>::infinity()));

    BOOST_TEST(!issignaling(std::numeric_limits<decimal_fast64_t>::quiet_NaN()));
    BOOST_TEST(issignaling(std::numeric_limits<decimal_fast64_t>::signaling_NaN()));
    BOOST_TEST(!issignaling(one));
    BOOST_TEST(!issignaling(std::numeric_limits<decimal_fast64_t>::infinity()));
    BOOST_TEST(!issignaling(-std::numeric_limits<decimal_fast64_t>::infinity()));

    #ifdef _MSC_VER

    BOOST_TEST(boost::decimal::isfinite(one));
    BOOST_TEST(!boost::decimal::isfinite(std::numeric_limits<decimal_fast64_t>::infinity()));
    BOOST_TEST(!boost::decimal::isfinite(std::numeric_limits<decimal_fast64_t>::quiet_NaN()));
    BOOST_TEST(!boost::decimal::isfinite(std::numeric_limits<decimal_fast64_t>::signaling_NaN()));

    #else

    BOOST_TEST(isfinite(one));
    BOOST_TEST(!isfinite(std::numeric_limits<decimal_fast64_t>::infinity()));
    BOOST_TEST(!isfinite(std::numeric_limits<decimal_fast64_t>::quiet_NaN()));
    BOOST_TEST(!isfinite(std::numeric_limits<decimal_fast64_t>::signaling_NaN()));

    #endif

    BOOST_TEST(isnormal(one));
    BOOST_TEST(!isnormal(std::numeric_limits<decimal_fast64_t>::infinity()));
    BOOST_TEST(!isnormal(std::numeric_limits<decimal_fast64_t>::quiet_NaN()));
    BOOST_TEST(!isnormal(std::numeric_limits<decimal_fast64_t>::signaling_NaN()));

    BOOST_TEST_EQ(fpclassify(one), FP_NORMAL);
    BOOST_TEST_EQ(fpclassify(-one), FP_NORMAL);
    BOOST_TEST_EQ(fpclassify(std::numeric_limits<decimal_fast64_t>::quiet_NaN()), FP_NAN);
    BOOST_TEST_EQ(fpclassify(std::numeric_limits<decimal_fast64_t>::signaling_NaN()), FP_NAN);
    BOOST_TEST_EQ(fpclassify(std::numeric_limits<decimal_fast64_t>::infinity()), FP_INFINITE);
    BOOST_TEST_EQ(fpclassify(-std::numeric_limits<decimal_fast64_t>::infinity()), FP_INFINITE);

    std::mt19937_64 rng(42);
    std::uniform_int_distribution<std::uint32_t> dist(1, 2);

    BOOST_TEST(isnan(detail::check_non_finite(one, std::numeric_limits<decimal_fast64_t>::quiet_NaN() * dist(rng))));
    BOOST_TEST(isnan(detail::check_non_finite(std::numeric_limits<decimal_fast64_t>::quiet_NaN() * dist(rng), one)));
    BOOST_TEST(isinf(detail::check_non_finite(one, std::numeric_limits<decimal_fast64_t>::infinity() * dist(rng))));
    BOOST_TEST(isinf(detail::check_non_finite(std::numeric_limits<decimal_fast64_t>::infinity() * dist(rng), one)));

    BOOST_TEST(isnan(decimal_fast64_t{std::numeric_limits<double>::quiet_NaN() * static_cast<double>(dist(rng))}));
}

#if !defined(__GNUC__) || (__GNUC__ != 7 && __GNUC__ != 8)
void test_unary_arithmetic()
{
    constexpr decimal_fast64_t one(1);
    BOOST_TEST(+one == one);
    if(!BOOST_TEST(-one != one))
    {
        // LCOV_EXCL_START
        std::cerr << "One: " << one
                  << "\nNeg: " << -one
                  << "\n    Bid: " << to_bid(one)
                  << "\nNeg Bid: " << to_bid(-one) << std::endl;
        // LCOV_EXCL_STOP
    }
}
#endif

void test_addition()
{
    // Case 1: The difference is more than the digits of accuracy
    constexpr decimal_fast64_t big_num(0b1, 20);
    constexpr decimal_fast64_t small_num(0b1, -20);
    BOOST_TEST_EQ(big_num + small_num, big_num);
    BOOST_TEST_EQ(small_num + big_num, big_num);

    // Case 2: Round the last digit of the significand
    constexpr decimal_fast64_t full_length_num {UINT64_C(1000000000000000), 0};
    constexpr decimal_fast64_t rounded_full_length_num(UINT64_C(1000000000000001), 0);
    constexpr decimal_fast64_t no_round(1, -1);
    constexpr decimal_fast64_t round(9, -1);
    BOOST_TEST_EQ(full_length_num + no_round, full_length_num);
    BOOST_TEST_EQ(full_length_num + round, rounded_full_length_num);

    // Case 3: Add away
    constexpr decimal_fast64_t one(1, 0);
    constexpr decimal_fast64_t two(2, 0);
    constexpr decimal_fast64_t three(3, 0);
    decimal_fast64_t mutable_one(1, 0);

    BOOST_TEST_EQ(one + one, two);
    BOOST_TEST_EQ(two + one, three);
    BOOST_TEST_EQ(one + one + one, three);

    // Pre- and post- increment
    BOOST_TEST_EQ(mutable_one, one);
    BOOST_TEST_EQ(mutable_one++, one);
    BOOST_TEST_EQ(++mutable_one, three);

    // Different orders of magnitude
    constexpr decimal_fast64_t ten(10, 0);
    constexpr decimal_fast64_t eleven(11, 0);
    BOOST_TEST_EQ(ten + one, eleven);

    constexpr decimal_fast64_t max_sig(9'999'999, 0);
    constexpr decimal_fast64_t max_plus_one(10'000'000, 0);
    BOOST_TEST_EQ(max_sig + one, max_plus_one);

    // Non-finite values
    constexpr decimal_fast64_t qnan_val(std::numeric_limits<decimal_fast64_t>::quiet_NaN());
    constexpr decimal_fast64_t snan_val(std::numeric_limits<decimal_fast64_t>::signaling_NaN());
    constexpr decimal_fast64_t inf_val(std::numeric_limits<decimal_fast64_t>::infinity());
    BOOST_TEST(isnan(qnan_val + one));
    BOOST_TEST(isnan(snan_val + one));
    BOOST_TEST(isnan(one + qnan_val));
    BOOST_TEST(isnan(one + snan_val));
    BOOST_TEST(isinf(inf_val + one));
    BOOST_TEST(isinf(one + inf_val));
    BOOST_TEST(isnan(inf_val + qnan_val));
    BOOST_TEST(isnan(qnan_val + inf_val));
}

void test_subtraction()
{
    // Case 1: The difference is more than the digits of accuracy
    constexpr decimal_fast64_t big_num(0b1, 20);
    constexpr decimal_fast64_t small_num(0b1, -20);
    BOOST_TEST_EQ(big_num - small_num, big_num);
    BOOST_TEST_EQ(small_num - big_num, -big_num);

    constexpr decimal_fast64_t one(1, 0);
    constexpr decimal_fast64_t two(2, 0);
    constexpr decimal_fast64_t three(3, 0);
    decimal_fast64_t mutable_three(3, 0);

    BOOST_TEST_EQ(two - one, one);
    BOOST_TEST_EQ(three - one - one, one);

    // Pre- and post- increment
    BOOST_TEST_EQ(mutable_three, three);
    BOOST_TEST_EQ(mutable_three--, three);
    BOOST_TEST_EQ(--mutable_three, one);

    // Different orders of magnitude
    constexpr decimal_fast64_t ten(10, 0);
    constexpr decimal_fast64_t eleven(11, 0);
    BOOST_TEST_EQ(eleven - one, ten);

    constexpr decimal_fast64_t max(9'999'999, 0);
    constexpr decimal_fast64_t max_plus_one(10'000'000, 0);
    BOOST_TEST_EQ(max_plus_one - one, max);

    // Non-finite values
    constexpr decimal_fast64_t qnan_val(std::numeric_limits<decimal_fast64_t>::quiet_NaN());
    constexpr decimal_fast64_t snan_val(std::numeric_limits<decimal_fast64_t>::signaling_NaN());
    constexpr decimal_fast64_t inf_val(std::numeric_limits<decimal_fast64_t>::infinity());
    BOOST_TEST(isnan(qnan_val - one));
    BOOST_TEST(isnan(snan_val - one));
    BOOST_TEST(isnan(one - qnan_val));
    BOOST_TEST(isnan(one - snan_val));
    BOOST_TEST(isinf(inf_val - one));
    BOOST_TEST(isinf(one - inf_val));
    BOOST_TEST(isnan(inf_val - qnan_val));
    BOOST_TEST(isnan(qnan_val - inf_val));
}

void test_multiplicatiom()
{
    constexpr decimal_fast64_t zero {0, 0};
    constexpr decimal_fast64_t one {1, 0};
    constexpr decimal_fast64_t two {2, 0};
    constexpr decimal_fast64_t four {4, 0};
    constexpr decimal_fast64_t eight {8, 0};

    BOOST_TEST_EQ(zero * one, zero);
    BOOST_TEST_EQ(zero * -one, -zero);
    BOOST_TEST_EQ(one * two, two);

    decimal_fast64_t pow_two {1, 0};
    BOOST_TEST_EQ(pow_two *= two, two);
    BOOST_TEST_EQ(pow_two *= two, four);
    BOOST_TEST_EQ(pow_two *= -two, -eight);

    // Non-finite values
    constexpr decimal_fast64_t qnan_val(std::numeric_limits<decimal_fast64_t>::quiet_NaN());
    constexpr decimal_fast64_t snan_val(std::numeric_limits<decimal_fast64_t>::signaling_NaN());
    constexpr decimal_fast64_t inf_val(std::numeric_limits<decimal_fast64_t>::infinity());
    BOOST_TEST(isnan(qnan_val * one));
    BOOST_TEST(isnan(snan_val * one));
    BOOST_TEST(isnan(one * qnan_val));
    BOOST_TEST(isnan(one * snan_val));
    BOOST_TEST(isinf(inf_val * one));
    BOOST_TEST(isinf(one * inf_val));
    BOOST_TEST(isnan(inf_val * qnan_val));
    BOOST_TEST(isnan(qnan_val * inf_val));
}

void test_div_mod()
{
    constexpr decimal_fast64_t zero {0, 0};
    constexpr decimal_fast64_t one {1, 0};
    constexpr decimal_fast64_t two {2, 0};
    constexpr decimal_fast64_t three {3, 0};
    constexpr decimal_fast64_t four {4, 0};
    constexpr decimal_fast64_t eight {8, 0};
    constexpr decimal_fast64_t half {5, -1};
    constexpr decimal_fast64_t quarter {25, -2};
    constexpr decimal_fast64_t eighth {125, -3};

    BOOST_TEST_EQ(two / one, two);
    BOOST_TEST_EQ(two % one, zero);
    BOOST_TEST_EQ(eight / four, two);
    BOOST_TEST_EQ(four / eight, half);
    BOOST_TEST_EQ(one / four, quarter);
    BOOST_TEST_EQ(one / eight, eighth);
    BOOST_TEST_EQ(three / two, one + half);

    // From https://en.cppreference.com/w/cpp/numeric/math/fmod
    BOOST_TEST_EQ(decimal_fast64_t(51, -1) % decimal_fast64_t(30, -1), decimal_fast64_t(21, -1));

    // Non-finite values
    constexpr decimal_fast64_t qnan_val(std::numeric_limits<decimal_fast64_t>::quiet_NaN());
    constexpr decimal_fast64_t snan_val(std::numeric_limits<decimal_fast64_t>::signaling_NaN());
    constexpr decimal_fast64_t inf_val(std::numeric_limits<decimal_fast64_t>::infinity());
    BOOST_TEST(isnan(qnan_val / one));
    BOOST_TEST(isnan(snan_val / one));
    BOOST_TEST(isnan(one / qnan_val));
    BOOST_TEST(isnan(one / snan_val));
    BOOST_TEST(isinf(inf_val / one));
    BOOST_TEST_EQ(one / inf_val, zero);
    BOOST_TEST(isnan(inf_val / qnan_val));
    BOOST_TEST(isnan(qnan_val / inf_val));

    // Mixed types
    BOOST_TEST(isnan(qnan_val / 1));
    BOOST_TEST(isnan(snan_val / 1));
    BOOST_TEST(isnan(1 / qnan_val));
    BOOST_TEST(isnan(1 / snan_val));
    BOOST_TEST(isinf(inf_val / 1));
    BOOST_TEST_EQ(1 / inf_val, zero);
}

template <typename T>
void test_construct_from_integer()
{
    constexpr decimal_fast64_t one(1, 0);
    BOOST_TEST_EQ(one, decimal_fast64_t(T(1)));

    constexpr decimal_fast64_t one_pow_eight(1, 8);
    BOOST_TEST_EQ(one_pow_eight, decimal_fast64_t(T(100'000'000)));
}

template <typename T>
void test_construct_from_float()
{
    constexpr decimal_fast64_t one(1, 0);
    decimal_fast64_t float_one(T(1));
    BOOST_TEST_EQ(one, float_one);

    constexpr decimal_fast64_t fraction(12345, -4);
    decimal_fast64_t float_frac(T(1.2345));
    BOOST_TEST_EQ(fraction, float_frac);

    constexpr decimal_fast64_t neg_frac(-98123, -4);
    decimal_fast64_t neg_float_frac(T(-9.8123));
    BOOST_TEST_EQ(neg_frac, neg_float_frac);
}

template <typename T>
void spot_check_addition(T a, T b, T res)
{
    decimal_fast64_t dec_a {a};
    decimal_fast64_t dec_b {b};
    decimal_fast64_t dec_res {res};

    if (!BOOST_TEST_EQ(dec_a + dec_b, dec_res))
    {
        // LCOV_EXCL_START
        std::cerr << "A + B: " << a + b
                  << "\nIn dec: " << decimal_fast64_t(a + b) << std::endl;
        // LCOV_EXCL_STOP
    }
}

void test_hash()
{
    decimal_fast64_t one {1, 0};
    decimal_fast64_t zero {0, 0};

    BOOST_TEST_NE(std::hash<decimal_fast64_t>{}(one), std::hash<decimal_fast64_t>{}(zero));
}

void test_shrink_significand()
{
    std::mt19937_64 rng(42);
    std::uniform_int_distribution<std::uint64_t> dist(100'000'000'000, 100'000'000'000);
    std::int32_t pow {};
    std::uint64_t sig {dist(rng)};

    detail::shrink_significand(sig, pow);
    BOOST_TEST_EQ(pow, 3);
}

int main()
{
    test_non_finite_values();

    #if !defined(__GNUC__) || (__GNUC__ != 7 && __GNUC__ != 8)
    test_unary_arithmetic();
    #endif

    test_construct_from_integer<int>();
    test_construct_from_integer<long>();
    test_construct_from_integer<long long>();

    test_construct_from_float<float>();
    test_construct_from_float<double>();
    #ifndef BOOST_DECIMAL_UNSUPPORTED_LONG_DOUBLE
    test_construct_from_float<long double>();
    #endif
    #if defined(BOOST_DECIMAL_HAS_FLOAT128) && (!defined(__clang_major__) || __clang_major__ >= 13)
    test_construct_from_float<__float128>();
    #endif

    test_comp();

    test_addition();
    test_subtraction();
    test_multiplicatiom();
    test_div_mod();

    test_hash();

    spot_check_addition(-1054191000, -920209700, -1974400700);
    spot_check_addition(353582500, -32044770, 321537730);
    spot_check_addition(989629100, 58451350, 1048080450);

    test_shrink_significand();

    return boost::report_errors();
}
