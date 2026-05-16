// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include "mini_to_chars.hpp"
#include "testing_config.hpp"
#include <boost/decimal.hpp>
#include <boost/core/lightweight_test.hpp>
#include <bitset>
#include <limits>
#include <random>
#include <climits>
#include <cmath>
#include <cerrno>
#include <iostream>
#include <iomanip>

using namespace boost::decimal;

#if !defined(BOOST_DECIMAL_REDUCE_TEST_DEPTH)
static constexpr auto N = static_cast<std::size_t>(1024); // Number of trials
#else
static constexpr auto N = static_cast<std::size_t>(1024 >> 4U); // Number of trials
#endif

// NOLINTNEXTLINE : Seed with a constant for repeatability
static std::mt19937_64 rng(42); // NOSONAR : Global rng is not const

template <typename Dec>
void test_nonfinite_samequantum()
{
    constexpr Dec one(1);
    constexpr Dec two(2);
    BOOST_TEST(samequantum(std::numeric_limits<Dec>::infinity(), std::numeric_limits<Dec>::infinity()));
    BOOST_TEST(samequantum(std::numeric_limits<Dec>::quiet_NaN(), std::numeric_limits<Dec>::quiet_NaN()));
    BOOST_TEST(samequantum(std::numeric_limits<Dec>::signaling_NaN(), std::numeric_limits<Dec>::signaling_NaN()));
    BOOST_TEST(!samequantum(std::numeric_limits<Dec>::infinity(), std::numeric_limits<Dec>::quiet_NaN()));
    BOOST_TEST(!samequantum(one, std::numeric_limits<Dec>::infinity()));
    BOOST_TEST(!samequantum(one, std::numeric_limits<Dec>::quiet_NaN()));
    BOOST_TEST(!samequantum(one, std::numeric_limits<Dec>::signaling_NaN()));
    BOOST_TEST(!samequantum(std::numeric_limits<Dec>::infinity(), one));
    BOOST_TEST(!samequantum(std::numeric_limits<Dec>::quiet_NaN(), one));
    BOOST_TEST(!samequantum(std::numeric_limits<Dec>::signaling_NaN(), one));
    BOOST_TEST(samequantum(one, two));
}

template <typename Dec>
void test_same_quantum()
{
    std::uniform_int_distribution<std::int64_t> sig(1'000'000, 9'999'999);
    std::uniform_int_distribution<std::int32_t> exp(std::numeric_limits<Dec>::min_exponent10 + 19,
                                                    std::numeric_limits<Dec>::max_exponent10 - 19);

    constexpr auto max_iter {std::is_same<Dec, decimal128_t>::value ? N / 4 : N};
    for (std::size_t i {}; i < max_iter; ++i)
    {
        auto exp1 {exp(rng)};
        auto exp2 {exp(rng)};

        const Dec val1 {sig(rng), exp1};
        const Dec val2 {sig(rng), exp2};


        if (exp1 == exp2)
        {
            if (!BOOST_TEST(samequantum(val1, val2)))
            {
                // LCOV_EXCL_START
                std::cerr << "Val 1: " << val1
                          << "\nVal 2: " << val2 << std::endl;
                // LCOV_EXCL_STOP
            }
        }
        else
        {
            if (!BOOST_TEST(!samequantum(val1, val2)))
            {
                // LCOV_EXCL_START
                std::cerr << "Val 1: " << val1
                          << "\nVal 2: " << val2 << std::endl;
                // LCOV_EXCL_STOP
            }
        }
    }
}

template <typename Dec>
void test_quantexp()
{
    // Loop through every possible exponent
    for (auto i {std::numeric_limits<Dec>::min_exponent10}; i < std::numeric_limits<Dec>::max_exponent10 - std::numeric_limits<Dec>::digits10; ++i)
    {
        const Dec val1 {1, i};

        if (isinf(val1))
        {
            continue; // LCOV_EXCL_LINE
        }
        else
        {
            if (!BOOST_TEST_EQ(quantexp(val1), i + detail::bias_v<Dec>))
            // LCOV_EXCL_START
            {
                std::cerr << "Val: " << val1
                          << "\nExp 1: " << i << std::endl;
            }
            // LCOV_EXCL_STOP
        }
    }
}

template <typename Dec>
void test_nonfinite_quantexp()
{
    BOOST_TEST_EQ(quantexp(std::numeric_limits<Dec>::infinity()), INT_MIN);
    BOOST_TEST_EQ(quantexp(-std::numeric_limits<Dec>::infinity()), INT_MIN);
    BOOST_TEST_EQ(quantexp(std::numeric_limits<Dec>::quiet_NaN()), INT_MIN);
    BOOST_TEST_EQ(quantexp(-std::numeric_limits<Dec>::quiet_NaN()), INT_MIN);
    BOOST_TEST_EQ(quantexp(std::numeric_limits<Dec>::signaling_NaN()), INT_MIN);
    BOOST_TEST_EQ(quantexp(-std::numeric_limits<Dec>::signaling_NaN()), INT_MIN);
}

template <typename Dec>
void test_quantize()
{
    using sig_type = typename Dec::significand_type;

    std::uniform_int_distribution<std::uint64_t> sig(1'000'000, 9'999'999);
    std::uniform_int_distribution<std::int32_t> exp(std::numeric_limits<Dec>::min_exponent10 + std::numeric_limits<Dec>::digits10 + 1,
                                                    std::numeric_limits<Dec>::max_exponent10 - std::numeric_limits<Dec>::digits10 - 1);

    constexpr auto max_iter {std::is_same<Dec, decimal128_t>::value ? N / 4 : N};
    for (std::size_t i {}; i < max_iter; ++i)
    {
        auto sig1 {static_cast<sig_type>(sig(rng))};
        auto sig2 {static_cast<sig_type>(sig(rng))};
        auto exp1 {exp(rng)};
        auto exp2 {exp(rng)};

        detail::normalize<Dec>(sig1, exp1);
        detail::normalize<Dec>(sig1, exp1);

        const Dec val1 {sig1, exp1};
        const Dec val2 {sig2, exp2};

        const Dec quantized_val {sig1, exp2};

        if (!BOOST_TEST_EQ(quantize(val1, val2), quantized_val))
        {
            // LCOV_EXCL_START
            std::cerr << std::setprecision(std::numeric_limits<Dec>::digits10)
                      << "Val 1: " << val1
                      << "\nVal 2: " << val2
                      << "\nQuant: " << quantized_val
                      << "\n Func: " << quantize(val1, val2) << std::endl;
            // LCOV_EXCL_STOP
        }
    }
}

template <typename Dec>
void test_nonfinite_quantize()
{
    const Dec one{1};
    std::uniform_int_distribution<std::int64_t> dist(1'000'000, 9'999'999);

    BOOST_TEST(isnan(quantize(std::numeric_limits<Dec>::quiet_NaN(), one)));
    BOOST_TEST(isnan(quantize(one, std::numeric_limits<Dec>::quiet_NaN())));
    BOOST_TEST(isnan(quantize(std::numeric_limits<Dec>::signaling_NaN(), one)));
    BOOST_TEST(isnan(quantize(one, std::numeric_limits<Dec>::signaling_NaN())));
    BOOST_TEST(isnan(quantize(std::numeric_limits<Dec>::infinity(), one)));
    BOOST_TEST(isnan(quantize(one, std::numeric_limits<Dec>::infinity())));
    BOOST_TEST(isinf(quantize(std::numeric_limits<Dec>::infinity() * dist(rng),
                              std::numeric_limits<Dec>::infinity() * dist(rng))));
}

int main()
{
    test_same_quantum<decimal32_t>();
    test_nonfinite_samequantum<decimal32_t>();
    test_quantexp<decimal32_t>();
    test_nonfinite_quantexp<decimal32_t>();
    test_quantize<decimal32_t>();
    test_nonfinite_quantize<decimal32_t>();

    test_same_quantum<decimal_fast32_t>();
    test_nonfinite_samequantum<decimal_fast32_t>();
    // decimal_fast32_t normalizes its value in the constructor,
    // so it will not match the values of the other types
    //test_quantexp<decimal_fast32_t>();
    test_nonfinite_quantexp<decimal_fast32_t>();
    test_quantize<decimal_fast32_t>();
    test_nonfinite_quantize<decimal_fast32_t>();

    test_same_quantum<decimal64_t>();
    test_nonfinite_samequantum<decimal64_t>();
    test_quantexp<decimal64_t>();
    test_nonfinite_quantexp<decimal64_t>();
    test_quantize<decimal64_t>();
    test_nonfinite_quantize<decimal64_t>();

    test_same_quantum<decimal128_t>();
    test_nonfinite_samequantum<decimal128_t>();
    test_quantexp<decimal128_t>();
    test_nonfinite_quantexp<decimal128_t>();
    test_quantize<decimal128_t>();
    test_nonfinite_quantize<decimal128_t>();

    return boost::report_errors();
}
