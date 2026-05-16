// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

// Propogates up from boost.math
#define _SILENCE_CXX23_DENORM_DEPRECATION_WARNING

#include "testing_config.hpp"
#include <iomanip>
#include <boost/decimal/decimal32_t.hpp>
#include <boost/decimal/decimal64_t.hpp>
#include <boost/decimal/decimal128_t.hpp>
#include <boost/decimal/decimal_fast32_t.hpp>
#include <boost/decimal/decimal_fast64_t.hpp>
#include <boost/decimal/decimal_fast128_t.hpp>
#include <boost/decimal/iostream.hpp>
#include <boost/decimal/cmath.hpp>

#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wold-style-cast"
#  pragma clang diagnostic ignored "-Wundef"
#  pragma clang diagnostic ignored "-Wconversion"
#  pragma clang diagnostic ignored "-Wsign-conversion"
#  pragma clang diagnostic ignored "-Wfloat-equal"
#  if __clang_major__ >= 20
#    pragma clang diagnostic ignored "-Wfortify-source"
#  endif
#elif defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wold-style-cast"
#  pragma GCC diagnostic ignored "-Wundef"
#  pragma GCC diagnostic ignored "-Wconversion"
#  pragma GCC diagnostic ignored "-Wsign-conversion"
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#  pragma GCC diagnostic ignored "-Wuseless-cast"
#endif

#include <boost/core/lightweight_test.hpp>
#include <boost/math/special_functions/next.hpp>

#include <iostream>
#include <random>
#include <cmath>
#include <array>

#if !defined(BOOST_DECIMAL_REDUCE_TEST_DEPTH) && !defined(_WIN32)
static constexpr auto N = static_cast<std::size_t>(128); // Number of trials
#else
static constexpr auto N = static_cast<std::size_t>(128 >> 4U); // Number of trials
#endif

static std::mt19937_64 rng(42);

using namespace boost::decimal;

template <typename Dec>
void test_fmax()
{
    
    std::uniform_int_distribution<int> dist(0, 10);

    BOOST_TEST_EQ(fmax(Dec(1), std::numeric_limits<Dec>::quiet_NaN() * Dec(dist(rng))), Dec(1));
    BOOST_TEST_EQ(fmax(std::numeric_limits<Dec>::quiet_NaN() * Dec(dist(rng)), Dec(1)), Dec(1));
    BOOST_TEST(isnan(fmax(std::numeric_limits<Dec>::quiet_NaN() * Dec(dist(rng)), std::numeric_limits<Dec>::quiet_NaN() * Dec(dist(rng)))));

    BOOST_TEST_EQ(fmax(Dec(1), Dec(0)), Dec(1));
    BOOST_TEST_EQ(fmax(Dec(-2), Dec(1)), Dec(1));

    // Mixed types
    BOOST_TEST_EQ(fmax(decimal128_t(1), Dec(0)), decimal128_t(1));
    BOOST_TEST_EQ(fmax(decimal128_t(-2), Dec(1)), decimal128_t(1));
}

template <typename Dec>
void test_isgreater()
{
    std::uniform_int_distribution<int> dist(1, 10);

    BOOST_TEST_EQ(isgreater(Dec(1), std::numeric_limits<Dec>::quiet_NaN() * dist(rng)), false);
    BOOST_TEST_EQ(isgreater(std::numeric_limits<Dec>::quiet_NaN() * dist(rng), Dec(1)), false);
    BOOST_TEST_EQ(isgreater(std::numeric_limits<Dec>::quiet_NaN() * dist(rng), std::numeric_limits<Dec>::quiet_NaN() * dist(rng)), false);
    BOOST_TEST_EQ(isgreater(std::numeric_limits<Dec>::infinity() * dist(rng), -std::numeric_limits<Dec>::infinity() * dist(rng)), true);

    BOOST_TEST_EQ(isgreater(Dec(1), Dec(0)), true);
    BOOST_TEST_EQ(isgreater(Dec(-2), Dec(1)), false);
    BOOST_TEST_EQ(isgreater(Dec(1), Dec(1)), false);
}

template <typename Dec>
void test_isgreaterequal()
{
    std::uniform_int_distribution<int> dist(1, 10);

    BOOST_TEST_EQ(isgreaterequal(Dec(1), std::numeric_limits<Dec>::quiet_NaN() * dist(rng)), false);
    BOOST_TEST_EQ(isgreaterequal(std::numeric_limits<Dec>::quiet_NaN() * dist(rng), Dec(1)), false);
    BOOST_TEST_EQ(isgreaterequal(std::numeric_limits<Dec>::quiet_NaN() * dist(rng), std::numeric_limits<Dec>::quiet_NaN()), false);
    BOOST_TEST_EQ(isgreaterequal(std::numeric_limits<Dec>::infinity() * dist(rng), -std::numeric_limits<Dec>::infinity()), true);

    BOOST_TEST_EQ(isgreaterequal(Dec(1), Dec(0)), true);
    BOOST_TEST_EQ(isgreaterequal(Dec(-2), Dec(1)), false);
    BOOST_TEST_EQ(isgreaterequal(Dec(1), Dec(1)), true);
}

template <typename Dec>
void test_fmin()
{
    BOOST_TEST_EQ(fmin(Dec(1), std::numeric_limits<Dec>::quiet_NaN()), Dec(1));
    BOOST_TEST_EQ(fmin(std::numeric_limits<Dec>::quiet_NaN(), Dec(1)), Dec(1));
    BOOST_TEST(isnan(fmin(std::numeric_limits<Dec>::quiet_NaN(), std::numeric_limits<Dec>::quiet_NaN())));
    BOOST_TEST_EQ(fmin(std::numeric_limits<Dec>::infinity(), -std::numeric_limits<Dec>::infinity()), -std::numeric_limits<Dec>::infinity());

    BOOST_TEST_EQ(fmin(Dec(1), Dec(0)), Dec(0));
    BOOST_TEST_EQ(fmin(Dec(-2), Dec(1)), Dec(-2));

    // Mixed types
    BOOST_TEST_EQ(fmin(decimal128_t(1), Dec(0)), decimal128_t(0));
    BOOST_TEST_EQ(fmin(decimal128_t(-2), Dec(1)), decimal128_t(-2));
}

template <typename Dec>
void test_isless()
{
    std::uniform_int_distribution<int> dist(1, 10);

    BOOST_TEST_EQ(isless(Dec(1), std::numeric_limits<Dec>::quiet_NaN() * dist(rng)), false);
    BOOST_TEST_EQ(isless(std::numeric_limits<Dec>::quiet_NaN() * dist(rng), Dec(1)), false);
    BOOST_TEST_EQ(isless(std::numeric_limits<Dec>::quiet_NaN() * dist(rng), std::numeric_limits<Dec>::quiet_NaN()), false);
    BOOST_TEST_EQ(isless(std::numeric_limits<Dec>::infinity() * dist(rng), -std::numeric_limits<Dec>::infinity()), false);

    BOOST_TEST_EQ(isless(Dec(1), Dec(0)), false);
    BOOST_TEST_EQ(isless(Dec(-2), Dec(1)), true);
    BOOST_TEST_EQ(isless(Dec(1), Dec(1)), false);
}

template <typename Dec>
void test_islessequal()
{
    std::uniform_int_distribution<int> dist(1, 10);

    BOOST_TEST_EQ(islessequal(Dec(1), std::numeric_limits<Dec>::quiet_NaN() * dist(rng)), false);
    BOOST_TEST_EQ(islessequal(std::numeric_limits<Dec>::quiet_NaN() * dist(rng), Dec(1)), false);
    BOOST_TEST_EQ(islessequal(std::numeric_limits<Dec>::quiet_NaN() * dist(rng), std::numeric_limits<Dec>::quiet_NaN()), false);
    BOOST_TEST_EQ(islessequal(std::numeric_limits<Dec>::infinity() * dist(rng), -std::numeric_limits<Dec>::infinity()), false);

    BOOST_TEST_EQ(islessequal(Dec(1), Dec(0)), false);
    BOOST_TEST_EQ(islessequal(Dec(-2), Dec(1)), true);
    BOOST_TEST_EQ(islessequal(Dec(1), Dec(1)), true);
}

template <typename Dec>
void test_islessgreater()
{
    std::uniform_int_distribution<int> dist(1, 10);

    BOOST_TEST_EQ(islessgreater(Dec(1), std::numeric_limits<Dec>::quiet_NaN() * dist(rng)), false);
    BOOST_TEST_EQ(islessgreater(std::numeric_limits<Dec>::quiet_NaN() * dist(rng), Dec(1)), false);
    BOOST_TEST_EQ(islessgreater(std::numeric_limits<Dec>::quiet_NaN() * dist(rng), std::numeric_limits<Dec>::quiet_NaN()), false);
    BOOST_TEST_EQ(islessgreater(std::numeric_limits<Dec>::infinity() * dist(rng), -std::numeric_limits<Dec>::infinity()), true);

    BOOST_TEST_EQ(islessgreater(Dec(1), Dec(0)), true);
    BOOST_TEST_EQ(islessgreater(Dec(-2), Dec(1)), true);
    BOOST_TEST_EQ(islessgreater(Dec(1), Dec(1)), false);
}

template <typename Dec>
void test_isunordered()
{
    BOOST_TEST_EQ(isunordered(Dec(1), std::numeric_limits<Dec>::quiet_NaN()), true);
    BOOST_TEST_EQ(isunordered(std::numeric_limits<Dec>::quiet_NaN(), Dec(1)), true);
    BOOST_TEST_EQ(isunordered(std::numeric_limits<Dec>::quiet_NaN(), std::numeric_limits<Dec>::quiet_NaN()), true);
    BOOST_TEST_EQ(isunordered(std::numeric_limits<Dec>::infinity(), -std::numeric_limits<Dec>::infinity()), false);

    BOOST_TEST_EQ(isunordered(Dec(1), Dec(0)), false);
    BOOST_TEST_EQ(isunordered(Dec(-2), Dec(1)), false);
    BOOST_TEST_EQ(isunordered(Dec(1), Dec(1)), false);
}

template <typename Dec>
void test_floor()
{
    
    std::uniform_int_distribution<int> dist(1, 1);

    BOOST_TEST(isnan(floor(std::numeric_limits<Dec>::quiet_NaN() * dist(rng))));
    BOOST_TEST(isnan(floor(-std::numeric_limits<Dec>::quiet_NaN() * dist(rng))));
    BOOST_TEST(isinf(floor(std::numeric_limits<Dec>::infinity() * dist(rng))));
    BOOST_TEST(isinf(floor(-std::numeric_limits<Dec>::infinity() * dist(rng))));
    BOOST_TEST_EQ(floor(Dec(0, 0) * dist(rng)), Dec(0, 0));
    BOOST_TEST_EQ(floor(Dec(-0, 0) * dist(rng)), Dec(-0, 0));

    BOOST_TEST_EQ(floor(Dec(27, -1) * dist(rng)), Dec(2, 0));
    BOOST_TEST_EQ(floor(Dec(-27, -1) * dist(rng)), Dec(-3, 0));
    BOOST_TEST_EQ(floor(Dec(27777, -4) * dist(rng)), Dec(2, 0));
    BOOST_TEST_EQ(floor(Dec(-27777, -4) * dist(rng)), Dec(-3, 0));
    BOOST_TEST_EQ(floor(Dec(2, 40)), Dec(2, 40));

    // Bigger numbers
    BOOST_TEST_EQ(floor(Dec(27777, -2) * dist(rng)), Dec(277, 0));
    BOOST_TEST_EQ(floor(Dec(-27777, -2) * dist(rng)), Dec(-277, 0));
    BOOST_TEST_EQ(floor(Dec(27777, -1) * dist(rng)), Dec(2777, 0));

    // Near zero
    BOOST_TEST_EQ(floor(Dec(3, -1) * dist(rng)), Dec(0, 0));
    BOOST_TEST_EQ(floor(Dec(-3, -1) * dist(rng)), Dec(-1, 0));
}

template <typename Dec>
void test_ceil()
{
    
    std::uniform_int_distribution<int> dist(0, 10);

    BOOST_TEST(isnan(ceil(std::numeric_limits<Dec>::quiet_NaN() * dist(rng))));
    BOOST_TEST(isnan(ceil(-std::numeric_limits<Dec>::quiet_NaN() * dist(rng))));
    BOOST_TEST(isinf(ceil(std::numeric_limits<Dec>::infinity() * dist(rng))));
    BOOST_TEST(isinf(ceil(-std::numeric_limits<Dec>::infinity() * dist(rng))));
    BOOST_TEST_EQ(ceil(Dec(0, 0) * dist(rng)), Dec(0, 0));
    BOOST_TEST_EQ(ceil(Dec(-0, 0) * dist(rng)), Dec(-0, 0));

    BOOST_TEST_EQ(ceil(Dec(27, -1)), Dec(3, 0));
    BOOST_TEST_EQ(ceil(Dec(-27, -1)), Dec(-2, 0));
    BOOST_TEST_EQ(ceil(Dec(27777, -4)), Dec(3, 0));
    BOOST_TEST_EQ(ceil(Dec(-27777, -4)), Dec(-2, 0));
    BOOST_TEST_EQ(ceil(Dec(2, 40)), Dec(2, 40));
    BOOST_TEST_EQ(ceil(Dec(2, 10)), Dec(2, 10));

    // Bigger numbers
    BOOST_TEST_EQ(ceil(Dec(27777, -2)), Dec(278, 0));
    BOOST_TEST_EQ(ceil(Dec(-27777, -2)), Dec(-277, 0));
    BOOST_TEST_EQ(ceil(Dec(27777, -1)), Dec(2778, 0));

    // Near zero
    BOOST_TEST_EQ(ceil(Dec(3, -1)), Dec(1, 0));
    BOOST_TEST_EQ(ceil(Dec(-3, -1)), Dec(0, 0));
}

template <typename Dec>
void test_trunc()
{
    BOOST_TEST(isnan(trunc(std::numeric_limits<Dec>::quiet_NaN())));
    BOOST_TEST(isnan(trunc(-std::numeric_limits<Dec>::quiet_NaN())));
    BOOST_TEST(isinf(trunc(std::numeric_limits<Dec>::infinity())));
    BOOST_TEST(isinf(trunc(-std::numeric_limits<Dec>::infinity())));
    BOOST_TEST_EQ(trunc(Dec(0, 0)), Dec(0, 0));
    BOOST_TEST_EQ(trunc(Dec(-0, 0)), Dec(-0, 0));

    BOOST_TEST_EQ(trunc(Dec(27, -1)), Dec(2, 0));
    BOOST_TEST_EQ(trunc(Dec(-27, -1)), Dec(-2, 0));
    BOOST_TEST_EQ(trunc(Dec(27777, -4)), Dec(2, 0));
    BOOST_TEST_EQ(trunc(Dec(-27777, -4)), Dec(-2, 0));
}

template <typename Dec>
void test_frexp10()
{
    auto expval = int {};

    BOOST_TEST_EQ(frexp10(Dec(0,0), &expval), 0);
    BOOST_TEST_EQ(expval, 0);

    expval = -1;
    BOOST_TEST_EQ(frexp10(std::numeric_limits<Dec>::quiet_NaN(), &expval), -1);
    BOOST_TEST_EQ(expval, 0);

    expval = -1;
    BOOST_TEST_EQ(frexp10(std::numeric_limits<Dec>::infinity(), &expval), -1);
    BOOST_TEST_EQ(expval, 0);

    BOOST_IF_CONSTEXPR (std::is_same<Dec, decimal32_t>::value)
    {
        BOOST_TEST_EQ(frexp10(Dec(10, 0), &expval), 1'000'000);
        BOOST_TEST_EQ(expval, -5);

        BOOST_TEST_EQ(frexp10(Dec(1'000'000, 5), &expval), 1'000'000);
        BOOST_TEST_EQ(expval, 5);

        BOOST_TEST_EQ(frexp10(Dec(-1'000'000, 5), &expval), 1'000'000);
        BOOST_TEST_EQ(expval, 5);
    }
    else BOOST_IF_CONSTEXPR (std::is_same<Dec, decimal64_t>::value)
    {
        BOOST_TEST_EQ(frexp10(Dec(10, 0), &expval), 1000000000000000);
        BOOST_TEST_EQ(expval, -14);

        BOOST_TEST_EQ(frexp10(Dec(1'000'000, 5), &expval), 1000000000000000);
        BOOST_TEST_EQ(expval, -4);

        BOOST_TEST_EQ(frexp10(Dec(-1'000'000, 5), &expval), 1000000000000000);
        BOOST_TEST_EQ(expval, -4);
    }
}

template <typename Dec>
void test_scalbn()
{
    BOOST_TEST(isinf(scalbn(std::numeric_limits<Dec>::infinity(), 1)));
    BOOST_TEST(isnan(scalbn(std::numeric_limits<Dec>::quiet_NaN(), 1)));
    BOOST_TEST_EQ(scalbn(Dec(0, 0), 1), Dec(0, 0));

    Dec one {1, 0};
    Dec ten {1, 1};
    Dec hundred {1, 2};

    BOOST_TEST_EQ(scalbn(one, 1), ten);
    BOOST_TEST_EQ(scalbn(one, 2), hundred);
    BOOST_TEST_EQ(scalbn(ten, 1), hundred);
    BOOST_TEST_EQ(scalbn(hundred, -1), ten);
    BOOST_TEST_EQ(scalbn(hundred, -2), one);
    BOOST_TEST_EQ(scalbn(hundred, 0), hundred);
    BOOST_TEST(isinf(scalbn(one, 10000)));
}

template <typename Dec>
void test_scalbln()
{
    BOOST_TEST(isinf(scalbln(std::numeric_limits<Dec>::infinity(), 1)));
    BOOST_TEST(isnan(scalbln(std::numeric_limits<Dec>::quiet_NaN(), 1)));
    BOOST_TEST_EQ(scalbln(Dec(0, 0), 1), Dec(0, 0));

    Dec one {1, 0};
    Dec ten {1, 1};
    Dec hundred {1, 2};

    BOOST_TEST_EQ(scalbln(one, 1), ten);
    BOOST_TEST_EQ(scalbln(one, 2), hundred);
    BOOST_TEST_EQ(scalbln(ten, 1), hundred);
    BOOST_TEST_EQ(scalbln(hundred, -1), ten);
    BOOST_TEST_EQ(scalbln(hundred, -2), one);
    BOOST_TEST_EQ(scalbln(hundred, 0), hundred);
    BOOST_TEST(isinf(scalbln(one, 10000)));
}

template <typename Dec>
void test_div_fmod()
{
    std::uniform_real_distribution<float> dist(0.0F, 1e30F);

    constexpr auto max_iter {std::is_same<Dec, decimal128_t>::value || std::is_same<Dec, decimal_fast128_t>::value ? N / 4 : N};
    for (std::size_t n {}; n < max_iter; ++n)
    {
        const auto val1 {dist(rng)};
        const auto val2 {dist(rng)};
        Dec d1 {val1};
        Dec d2 {val2};

        auto float_div {val1 / val2};
        auto decimal_div {static_cast<float>(d1 / d2)};
        auto float_fmod {std::fmod(val1, val2)};
        auto decimal_fmod {static_cast<float>(fmod(d1, d2))};

        // Decimal types are all higher precision than float
        if (!(BOOST_TEST(std::fabs(boost::math::float_distance(float_fmod, decimal_fmod)) < 1e7) &&
              BOOST_TEST(std::fabs(boost::math::float_distance(float_div, decimal_div)) < 20)))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << d1
                      << "\nVal 2: " << val2
                      << "\nDec 2: " << d2
                      << "\nVal div: " << float_div
                      << "\nDec div: " << decimal_div
                      << "\nDist: " << boost::math::float_distance(float_div, decimal_div)
                      << "\nVal fmod: " << float_fmod
                      << "\nDec fmod: " << decimal_fmod
                      << "\nDist: " << boost::math::float_distance(float_fmod, decimal_fmod) << std::endl;
            // LCOV_EXCL_STOP
        }
    }
}

template <typename Dec>
void test_copysign()
{
    std::uniform_real_distribution<float> dist(0.0F, 1e30F);

    constexpr auto max_iter {std::is_same<Dec, decimal128_t>::value || std::is_same<Dec, decimal_fast128_t>::value ? N / 4 : N};
    for (std::size_t n {}; n < max_iter; ++n)
    {
        const auto val1 {dist(rng)};
        const auto val2 {dist(rng)};
        Dec d1 {val1};
        Dec d2 {val2};

        auto ret_val {std::copysign(val1, val2)};
        auto ret_dec {static_cast<float>(copysign(d1, d2))};

        if (!BOOST_TEST(std::fabs(boost::math::float_distance(ret_val, ret_dec)) < 20))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << d1
                      << "\nVal 2: " << val2
                      << "\nDec 2: " << d2
                      << "\nRet val: " << ret_val
                      << "\nRet dec: " << ret_dec << std::endl;
            // LCOV_EXCL_STOP
        }
    }
}

template <typename Dec>
void test_fma()
{
    if (!BOOST_TEST_EQ(Dec(1, -1) * Dec(1, 1), Dec(1, 0)))
    {
        // LCOV_EXCL_START
        std::cerr << std::setprecision(std::numeric_limits<Dec>::digits10)
                  << "   Mul: " << Dec(1, -1) * Dec(1, 1)
                  << "\nActual: " << Dec(1, 0) << std::endl;
        // LCOV_EXCL_STOP
    }

    BOOST_TEST_EQ(Dec(1, 0) + Dec(-1, 0), Dec(0, 0));
    BOOST_TEST_EQ(fma(Dec(1, -1), Dec(1, 1), Dec(-1, 0)), Dec(0, 0));

    std::uniform_real_distribution<double> dist(-1e3, 1e3);

    constexpr auto max_iter {std::is_same<Dec, decimal128_t>::value || std::is_same<Dec, decimal_fast128_t>::value ? N / 4 : N};
    for (std::size_t n {}; n < max_iter; ++n)
    {
        const auto val1 {dist(rng)};
        const auto val2 {dist(rng)};
        const auto val3 {dist(rng)};
        Dec d1 {val1};
        Dec d2 {val2};
        Dec d3 {val3};

        auto fma_val {fma(d1, d2, d3)};
        auto naive_val {(d1 * d2) + d3};

        if (!BOOST_TEST(fabs(fma_val - naive_val) < Dec(1, 5)))
        {
            // LCOV_EXCL_START
            std::cerr << "Dec 1: " << d1
                      << "\nDec 2: " << d2
                      << "\nDec 3: " << d3
                      << "\nfma val: " << fma_val
                      << "\nNaive val: " << naive_val
                      << "\nDist: " << fabs(fma_val - naive_val) << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    // Edge cases
    BOOST_TEST(isinf(fma(std::numeric_limits<Dec>::infinity(), Dec(dist(rng)), Dec(dist(rng)))));
    BOOST_TEST(isnan(fma(std::numeric_limits<Dec>::quiet_NaN(), Dec(dist(rng)), Dec(dist(rng)))));
    BOOST_TEST(isinf(fma(Dec(dist(rng)), std::numeric_limits<Dec>::infinity(), Dec(dist(rng)))));
    BOOST_TEST(isnan(fma(Dec(dist(rng)), std::numeric_limits<Dec>::quiet_NaN(), Dec(dist(rng)))));
    BOOST_TEST(isinf(fma(Dec(dist(rng)), Dec(dist(rng)), std::numeric_limits<Dec>::infinity())));
    BOOST_TEST(isnan(fma(Dec(dist(rng)), Dec(dist(rng)), std::numeric_limits<Dec>::quiet_NaN())));
}

template <typename Dec>
void test_modf()
{
    Dec ptr {};
    BOOST_TEST_EQ(modf(Dec(123.45), &ptr), Dec(0.45));
    BOOST_TEST_EQ(ptr, Dec(123));

    BOOST_TEST_EQ(modf(std::numeric_limits<Dec>::infinity(), &ptr), Dec(0));
    BOOST_TEST_EQ(ptr, std::numeric_limits<Dec>::infinity());

    BOOST_TEST(modf(std::numeric_limits<Dec>::quiet_NaN(), &ptr) != std::numeric_limits<Dec>::quiet_NaN());
    BOOST_TEST(ptr != std::numeric_limits<Dec>::quiet_NaN());
}

template <typename Dec>
void test_fdim()
{
    std::uniform_real_distribution<float> dist(1.0F, 1e5F);

    constexpr auto max_iter {std::is_same<Dec, decimal128_t>::value || std::is_same<Dec, decimal_fast128_t>::value ? N / 4 : N};
    for (std::size_t n {}; n < max_iter; ++n)
    {
        const auto val1 {dist(rng)};
        const auto val2 {dist(rng)};
        Dec d1 {val1};
        Dec d2 {val2};

        auto ret_val {std::fdim(val1, val2)};
        auto ret_dec {static_cast<float>(fdim(d1, d2))};

        if (ret_val == 0 || ret_dec == 0)
        {
            BOOST_TEST_EQ(ret_val, ret_dec);
        }
        else if (!BOOST_TEST(std::fabs(ret_val - ret_dec) < 1))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << d1
                      << "\nVal 2: " << val2
                      << "\nDec 2: " << d2
                      << "\nRet val: " << ret_val
                      << "\nRet dec: " << ret_dec << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    BOOST_TEST(isinf(fdim(std::numeric_limits<Dec>::infinity() * Dec(dist(rng)), Dec(1))));
    BOOST_TEST(isnan(fdim(std::numeric_limits<Dec>::quiet_NaN() * Dec(dist(rng)), Dec(1))));
    BOOST_TEST(isnan(fdim(Dec(1), std::numeric_limits<Dec>::quiet_NaN() * Dec(dist(rng)))));
    BOOST_TEST_EQ(fdim(Dec(1), Dec(1)), Dec(0));
}

// Macro if constexpr throws warning in C++14 mode
#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable : 4127)
#endif

template <typename Dec>
void test_ilogb()
{
    BOOST_DECIMAL_IF_CONSTEXPR (std::is_same<Dec, decimal32_t>::value || std::is_same<Dec, decimal_fast32_t>::value)
    {
        BOOST_TEST_EQ(ilogb(Dec(1, 0)), 101);
        BOOST_TEST_EQ(ilogb(Dec(10, 0)), 102);
    }
    else BOOST_DECIMAL_IF_CONSTEXPR (std::is_same<Dec, decimal64_t>::value || std::is_same<Dec, decimal_fast64_t>::value)
    {
        BOOST_TEST_EQ(ilogb(Dec(1, 0)), 398);
        BOOST_TEST_EQ(ilogb(Dec(10, 0)), 399);
    }
    else
    {
        BOOST_TEST_EQ(ilogb(Dec(1, 0)), 6176);
        BOOST_TEST_EQ(ilogb(Dec(10, 0)), 6177);
    }

    std::uniform_int_distribution<int> dist(1, 2);

    BOOST_TEST_EQ(ilogb(Dec(0 * dist(rng))), FP_ILOGB0);
    BOOST_TEST_EQ(ilogb(std::numeric_limits<Dec>::infinity() * dist(rng)), INT_MAX);
    BOOST_TEST_EQ(ilogb(std::numeric_limits<Dec>::quiet_NaN() * dist(rng)), FP_ILOGBNAN);
}

template <typename Dec>
void test_logb()
{
    std::uniform_int_distribution<int> dist(1, 1);

    BOOST_DECIMAL_IF_CONSTEXPR (std::is_same<Dec, decimal32_t>::value || std::is_same<Dec, decimal_fast32_t>::value)
    {
        BOOST_TEST_EQ(ilogb(Dec(1 * dist(rng), 0)), Dec(101));
        BOOST_TEST_EQ(ilogb(Dec(10 * dist(rng), 0)), Dec(102));
    }
    else BOOST_DECIMAL_IF_CONSTEXPR (std::is_same<Dec, decimal64_t>::value || std::is_same<Dec, decimal_fast64_t>::value)
    {
        BOOST_TEST_EQ(ilogb(Dec(1 * dist(rng), 0)), Dec(398));
        BOOST_TEST_EQ(ilogb(Dec(10 * dist(rng), 0)), Dec(399));
    }
    else
    {
        BOOST_TEST_EQ(ilogb(Dec(1 * dist(rng), 0)), 6176);
        BOOST_TEST_EQ(ilogb(Dec(10 * dist(rng), 0)), 6177);
    }

    BOOST_TEST_EQ(logb(Dec(0 * dist(rng))), -std::numeric_limits<Dec>::infinity());
    BOOST_TEST_EQ(logb(std::numeric_limits<Dec>::infinity() * dist(rng)), std::numeric_limits<Dec>::infinity());
    BOOST_TEST(isnan(logb(std::numeric_limits<Dec>::quiet_NaN() * dist(rng))));
}

#ifdef _MSC_VER
#  pragma warning(pop)
#endif

template <typename Dec>
void test_sqrt()
{
    using comp_type = std::conditional_t<std::is_same<Dec, decimal32_t>::value || std::is_same<Dec, decimal_fast32_t>::value, float, double>;
    std::uniform_real_distribution<comp_type> dist(0, 1e5);

    constexpr auto max_iter {std::is_same<Dec, decimal128_t>::value || std::is_same<Dec, decimal_fast128_t>::value ? N / 4 : N};
    for (std::size_t n {}; n < max_iter; ++n)
    {
        const auto val1 {dist(rng)};
        Dec d1 {val1};

        auto ret_val {std::sqrt(val1)};
        auto ret_dec {static_cast<comp_type>(sqrt(d1))};

        if (!BOOST_TEST(boost::math::float_distance(ret_val, ret_dec) < 15))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << d1
                      << "\nRet val: " << std::setprecision(std::numeric_limits<comp_type>::digits10 + 2) << ret_val
                      << "\nRet dec: " << std::setprecision(std::numeric_limits<comp_type>::digits10 + 2) << ret_dec
                      << "\nEps: " << boost::math::float_distance(ret_val, ret_dec) << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    Dec inf {std::numeric_limits<Dec>::infinity() * static_cast<int>(dist(rng))};
    Dec nan {std::numeric_limits<Dec>::quiet_NaN() * static_cast<int>(dist(rng))};
    Dec zero {0 * static_cast<int>(dist(rng))};
    Dec neg_num {-static_cast<int>(dist(rng))};
    BOOST_TEST(isinf(sqrt(inf)));
    BOOST_TEST(isnan(sqrt(-inf)));
    BOOST_TEST(isnan(sqrt(nan)));
    BOOST_TEST(isnan(sqrt(-nan)));
    BOOST_TEST_EQ(sqrt(zero), zero);
    BOOST_TEST(isnan(sqrt(neg_num)));
}

template <typename Dec>
void test_two_val_hypot()
{
    std::uniform_real_distribution<float> dist(1.0F, 1e5F);

    constexpr auto max_iter {std::is_same<Dec, decimal128_t>::value || std::is_same<Dec, decimal_fast128_t>::value ? 4 : N};
    for (std::size_t n {}; n < max_iter; ++n)
    {
        const auto val1 {dist(rng)};
        const auto val2 {dist(rng)};
        Dec d1 {val1};
        Dec d2 {val2};

        auto ret_val {std::hypot(val1, val2)};
        auto ret_dec {static_cast<float>(hypot(d1, d2))};

        if (ret_val == 0 || ret_dec == 0)
        {
            BOOST_TEST_EQ(ret_val, ret_dec); // LCOV_EXCL_LINE
        }
        else if (!BOOST_TEST(std::fabs(ret_val - ret_dec) < 1500))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << d1
                      << "\nVal 2: " << val2
                      << "\nDec 2: " << d2
                      << "\nRet val: " << ret_val
                      << "\nRet dec: " << ret_dec
                      << "\nEps: " << std::fabs(ret_val - ret_dec) / std::numeric_limits<float>::epsilon() << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    const auto big_val {static_cast<double>(dist(rng))};
    BOOST_TEST_EQ(hypot(Dec(big_val), Dec(big_val * 1e100)), Dec(big_val * 1e100));

    Dec inf {std::numeric_limits<Dec>::infinity() * static_cast<int>(dist(rng))};
    Dec nan {std::numeric_limits<Dec>::quiet_NaN() * static_cast<int>(dist(rng))};
    Dec zero {0 * static_cast<int>(dist(rng))};
    BOOST_TEST_EQ(hypot(zero, Dec(1)), Dec(1));
    BOOST_TEST_EQ(hypot(Dec(1), zero), Dec(1));
    BOOST_TEST(isinf(hypot(inf, Dec(1))));
    BOOST_TEST(isinf(hypot(Dec(1), inf)));
    BOOST_TEST(isnan(hypot(nan, Dec(1))));
    BOOST_TEST(isnan(hypot(Dec(1), nan)));
}

template <typename Dec1, typename Dec2>
void test_mixed_two_val_hypot()
{
    std::uniform_real_distribution<float> dist(1.0F, 1e5F);

    constexpr auto max_iter {std::is_same<Dec2, decimal128_t>::value || std::is_same<Dec2, decimal_fast128_t>::value ? 4 : N};
    for (std::size_t n {}; n < max_iter; ++n)
    {
        const auto val1 {dist(rng)};
        const auto val2 {dist(rng)};
        Dec1 d1 {val1};
        Dec2 d2 {val2};

        auto ret_val {std::hypot(val1, val2)};
        auto ret_dec {static_cast<float>(hypot(d1, d2))};

        if (ret_val == 0 || ret_dec == 0)
        {
            BOOST_TEST_EQ(ret_val, ret_dec); // LCOV_EXCL_LINE
        }
        else if (!BOOST_TEST(std::fabs(ret_val - ret_dec) < 1500))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << d1
                      << "\nVal 2: " << val2
                      << "\nDec 2: " << d2
                      << "\nRet val: " << ret_val
                      << "\nRet dec: " << ret_dec
                      << "\nEps: " << std::fabs(ret_val - ret_dec) / std::numeric_limits<float>::epsilon() << std::endl;
            // LCOV_EXCL_STOP
        }
    }
}

#if defined(__cpp_lib_hypot) && __cpp_lib_hypot >= 201603L

template <typename Dec>
void test_three_val_hypot()
{
    
    std::uniform_real_distribution<float> dist(1.0F, 1e5F);

    constexpr auto max_iter {std::is_same<Dec, decimal128_t>::value || std::is_same<Dec, decimal_fast128_t>::value ? 4 : N};
    for (std::size_t n {}; n < max_iter; ++n)
    {
        const auto val1 {dist(rng)};
        const auto val2 {dist(rng)};
        const auto val3 {dist(rng)};
        Dec d1 {val1};
        Dec d2 {val2};
        Dec d3 {val3};

        auto ret_val {std::hypot(val1, val2, val3)};
        auto ret_dec {static_cast<float>(hypot(d1, d2, d3))};

        if (!BOOST_TEST(std::fabs(ret_val - ret_dec) < 1500))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << d1
                      << "\nVal 2: " << val2
                      << "\nDec 2: " << d2
                      << "\nVal 3: " << val3
                      << "\nDec 3: " << d3
                      << "\nRet val: " << ret_val
                      << "\nRet dec: " << ret_dec
                      << "\nEps: " << std::fabs(ret_val - ret_dec) / std::numeric_limits<float>::epsilon() << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    Dec inf {std::numeric_limits<Dec>::infinity() * static_cast<int>(dist(rng))};
    Dec nan {std::numeric_limits<Dec>::quiet_NaN() * static_cast<int>(dist(rng))};
    Dec zero {0 * static_cast<int>(dist(rng))};
    BOOST_TEST(isinf(hypot(inf, Dec(1), Dec(1))));
    BOOST_TEST(isinf(hypot(Dec(1), inf, Dec(1))));
    BOOST_TEST(isinf(hypot(Dec(1), Dec(1), inf)));
    BOOST_TEST(isnan(hypot(nan, Dec(1), Dec(1))));
    BOOST_TEST(isnan(hypot(Dec(1), nan, Dec(1))));
    BOOST_TEST(isnan(hypot(Dec(1), Dec(1), nan)));
}

#else

template <typename Dec>
void test_three_val_hypot()
{
    // Empty because we can't test against it yet
}

#endif

template <typename Dec>
void test_rint()
{
    std::uniform_real_distribution<float> dist(-1e20F, 1e20F);

    constexpr auto max_iter {std::is_same<Dec, decimal128_t>::value || std::is_same<Dec, decimal_fast128_t>::value ? N / 4 : N};

    for (std::size_t n {}; n < max_iter; ++n)
    {
        const auto val1 {dist(rng)};
        Dec d1 {val1};

        auto ret_val {static_cast<float>(std::rint(val1))};
        auto ret_dec {static_cast<float>(rint(d1))};

        if(!BOOST_TEST(std::fabs(boost::math::float_distance(val1, ret_dec)) < 10))
        {
            // LCOV_EXCL_START
            std::cerr << std::setprecision(std::numeric_limits<Dec>::digits10)
                      << "Val 1: " << val1
                      << "\nDec 1: " << d1
                      << "\nRet val: " << ret_val
                      << "\nRet dec: " << ret_dec
                      << "\nDist: " << boost::math::float_distance(val1, ret_dec) << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    std::uniform_real_distribution<float> dist2(-1e5F, 1e5F);

    for (std::size_t n {}; n < max_iter; ++n)
    {
        const auto val1 {dist2(rng)};
        Dec d1 {val1};

        auto ret_val {static_cast<float>(std::rint(val1))};
        auto ret_dec {static_cast<float>(rint(d1))};

        // Difference in default rounding mode
        // Float goes to even while decimal is to nearest from zero
        float iptr {};
        const auto frac {std::modf(val1, &iptr)};
        if (std::abs(frac) <= 0.5F && std::abs(frac) >= 0.49F)
        {
            continue;
        }

        if (!BOOST_TEST_EQ(ret_val, ret_dec))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << d1
                      << "\nRet val: " << ret_val
                      << "\nRet dec: " << ret_dec
                      << "\nEps: " << std::fabs(ret_val - ret_dec) / std::numeric_limits<float>::epsilon() << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    BOOST_TEST(isinf(rint(std::numeric_limits<Dec>::infinity() * Dec(dist(rng)))));
    BOOST_TEST(isnan(rint(std::numeric_limits<Dec>::quiet_NaN() * Dec(dist(rng)))));
    BOOST_TEST_EQ(abs(rint(Dec(0) * Dec(dist(rng)))), Dec(0));
    BOOST_TEST_EQ(rint(Dec(0) * Dec(dist(rng)) + Dec(1, -20)), Dec(0));
    BOOST_TEST_EQ(rint(Dec(0) * Dec(dist(rng)) + Dec(1U, -20, true)), Dec(0U, 0, true));
}

template <typename Dec>
void test_lrint()
{
    using std::abs;

    std::uniform_real_distribution<float> dist(-1e5F, 1e5F);

    constexpr auto max_iter {std::is_same<Dec, decimal128_t>::value || std::is_same<Dec, decimal_fast128_t>::value ? N / 4 : N};
    for (std::size_t n {}; n < max_iter; ++n)
    {
        const auto val1 {dist(rng)};
        Dec d1 {val1};

        auto ret_val {std::lrint(val1)};
        auto ret_dec {lrint(d1)};

        // Difference in rounding mode at 0.5
        if (abs(ret_dec) == abs(ret_val) + 1)
        {
            // LCOV_EXCL_START

            float iptr;
            const auto frac = std::modf(val1, &iptr);
            if (abs(abs(frac) - 0.5F) < 0.01F)
            {
                continue;
            }
            else
            {
                std::cerr << "Frac: " << frac
                          << "\nDist: " << std::fabs(frac - 0.5F) / std::numeric_limits<float>::epsilon() << std::endl;
            }

            // LCOV_EXCL_STOP
        }

        if (!BOOST_TEST_EQ(ret_val, ret_dec))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << d1
                      << "\nRet val: " << ret_val
                      << "\nRet dec: " << ret_dec << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    std::uniform_real_distribution<float> dist2(-1e5F, 1e5F);

    for (std::size_t n {}; n < max_iter; ++n)
    {
        const auto val1 {dist2(rng)};
        Dec d1 {val1};

        auto ret_val {std::lrint(val1)};
        auto ret_dec {lrint(d1)};

        // Difference in significant figures
        float iptr {};
        const auto frac {std::modf(val1, &iptr)};
        if (std::abs(frac) <= 0.5F && std::abs(frac) >= 0.49F)
        {
            continue;
        }

        if (!BOOST_TEST_EQ(ret_val, ret_dec))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << d1
                      << "\nRet val: " << ret_val
                      << "\nRet dec: " << ret_dec << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    BOOST_TEST_EQ(lrint(std::numeric_limits<Dec>::infinity() * Dec(dist(rng))), LONG_MIN);
    BOOST_TEST_EQ(lrint(std::numeric_limits<Dec>::quiet_NaN() * Dec(dist(rng))), LONG_MIN);
    BOOST_TEST_EQ(lrint(Dec(0) * Dec(dist(rng))), 0);
    BOOST_TEST_EQ(lrint(Dec(0) * Dec(dist(rng)) + Dec(1, -20)), 0);
    BOOST_TEST_EQ(lrint(Dec(0) * Dec(dist(rng)) + Dec(1U, -20, true)), 0);
}

template <typename Dec>
void test_llrint()
{
    std::uniform_real_distribution<float> dist(-1e20F, 1e20F);

    constexpr auto max_iter {std::is_same<Dec, decimal128_t>::value || std::is_same<Dec, decimal_fast128_t>::value ? N / 4 : N};

    std::uniform_real_distribution<float> dist2(-1e5F, 1e5F);

    for (std::size_t n {}; n < max_iter; ++n)
    {
        const auto val1 {dist2(rng)};
        Dec d1 {val1};

        auto ret_val {std::llrint(val1)};
        auto ret_dec {llrint(d1)};

        // Difference in significant figures
        float iptr {};
        const auto frac {std::modf(val1, &iptr)};
        if (std::abs(frac) <= 0.5F && std::abs(frac) >= 0.49F)
        {
            continue;
        }

        if (!BOOST_TEST_EQ(ret_val, ret_dec))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << d1
                      << "\nRet val: " << ret_val
                      << "\nRet dec: " << ret_dec << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    BOOST_TEST_EQ(llrint(std::numeric_limits<Dec>::infinity() * Dec(dist(rng))), LLONG_MIN);
    BOOST_TEST_EQ(llrint(std::numeric_limits<Dec>::quiet_NaN() * Dec(dist(rng))), LLONG_MIN);
    BOOST_TEST_EQ(llrint(Dec(0) * Dec(dist(rng))), 0);
    BOOST_TEST_EQ(llrint(Dec(0) * Dec(dist(rng)) + Dec(1, -20)), 0);
    BOOST_TEST_EQ(llrint(Dec(0) * Dec(dist(rng)) + Dec(1U, -20, true)), 0);
}

template <typename Dec>
void test_nearbyint()
{
    std::uniform_real_distribution<float> dist(-1e20F, 1e20F);

    constexpr auto max_iter {std::is_same<Dec, decimal128_t>::value || std::is_same<Dec, decimal_fast128_t>::value ? N / 4 : N};
    for (std::size_t n {}; n < max_iter; ++n)
    {
        const auto val1 {dist(rng)};
        Dec d1 {val1};

        auto ret_val {static_cast<float>(std::nearbyint(val1))};
        auto ret_dec {static_cast<float>(nearbyint(d1))};

        if(!BOOST_TEST(std::fabs(boost::math::float_distance(val1, ret_dec)) < 10))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << d1
                      << "\nRet val: " << ret_val
                      << "\nRet dec: " << ret_dec
                      << "\nDist: " << boost::math::float_distance(val1, ret_dec) << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    std::uniform_real_distribution<float> dist2(-1e5F, 1e5F);

    for (std::size_t n {}; n < max_iter; ++n)
    {
        const auto val1 {dist2(rng)};
        Dec d1 {val1};

        auto ret_val {static_cast<float>(std::nearbyint(val1))};
        auto ret_dec {static_cast<float>(nearbyint(d1))};

        // Difference in default rounding mode
        // Float goes to even while decimal is to nearest from zero
        float iptr {};
        const auto frac {std::modf(val1, &iptr)};
        if (std::abs(frac) <= 0.5F && std::abs(frac) >= 0.49F)
        {
            continue;
        }

        if (!BOOST_TEST_EQ(ret_val, ret_dec))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << d1
                      << "\nRet val: " << ret_val
                      << "\nRet dec: " << ret_dec
                      << "\nEps: " << std::fabs(ret_val - ret_dec) / std::numeric_limits<float>::epsilon() << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    BOOST_TEST(isinf(nearbyint(std::numeric_limits<Dec>::infinity() * Dec(dist(rng)))));
    BOOST_TEST(isnan(nearbyint(std::numeric_limits<Dec>::quiet_NaN() * Dec(dist(rng)))));
    BOOST_TEST_EQ(abs(nearbyint(Dec(0) * Dec(dist(rng)))), Dec(0));
    BOOST_TEST_EQ(nearbyint(Dec(0) * Dec(dist(rng)) + Dec(1, -20)), Dec(0));
    BOOST_TEST_EQ(nearbyint(Dec(0) * Dec(dist(rng)) + Dec(1U, -20, true)), Dec(0U, 0, true));
}

template <typename Dec>
void test_round()
{
    std::uniform_real_distribution<float> dist(-1e5F, 1e5F);

    constexpr auto max_iter {std::is_same<Dec, decimal128_t>::value || std::is_same<Dec, decimal_fast128_t>::value ? N / 4 : N};
    for (std::size_t n {}; n < max_iter; ++n)
    {
        const auto val1 {dist(rng)};
        Dec d1 {val1};

        auto ret_val {static_cast<float>(std::round(val1))};
        auto ret_dec {static_cast<float>(round(d1))};

        // Difference in default rounding mode
        // Float goes to even while decimal is to nearest from zero
        float iptr {};
        const auto frac {std::modf(val1, &iptr)};
        if (std::abs(frac) <= 0.5F && std::abs(frac) >= 0.49F)
        {
            continue;
        }

        if (!BOOST_TEST_EQ(ret_val, ret_dec))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << d1
                      << "\nRet val: " << ret_val
                      << "\nRet dec: " << ret_dec
                      << "\nEps: " << std::fabs(ret_val - ret_dec) / std::numeric_limits<float>::epsilon() << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    BOOST_TEST(isinf(round(std::numeric_limits<Dec>::infinity() * Dec(dist(rng)))));
    BOOST_TEST(isnan(round(std::numeric_limits<Dec>::quiet_NaN() * Dec(dist(rng)))));
    BOOST_TEST_EQ(abs(round(Dec(0) * Dec(dist(rng)))), Dec(0));
    BOOST_TEST_EQ(round(Dec(0) * Dec(dist(rng)) + Dec(1, -20)), Dec(0));
    BOOST_TEST_EQ(round(Dec(0) * Dec(dist(rng)) + Dec(1U, -20, true)), Dec(0U, 0, false));
}

template <typename Dec>
void test_lround()
{
    std::uniform_real_distribution<float> dist(-1e20F, 1e20F);

    constexpr auto max_iter {std::is_same<Dec, decimal128_t>::value || std::is_same<Dec, decimal_fast128_t>::value ? N / 4 : N};

    std::uniform_real_distribution<float> dist2(-1e5F, 1e5F);

    for (std::size_t n {}; n < max_iter; ++n)
    {
        const auto val1 {dist2(rng)};
        Dec d1 {val1};

        auto ret_val {std::lround(val1)};
        auto ret_dec {lround(d1)};

        // Difference in significant figures
        float iptr {};
        const auto frac {std::modf(val1, &iptr)};
        if (std::abs(frac) <= 0.5F && std::abs(frac) >= 0.49F)
        {
            continue;
        }

        if (!BOOST_TEST_EQ(ret_val, ret_dec))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << d1
                      << "\nRet val: " << ret_val
                      << "\nRet dec: " << ret_dec << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    BOOST_TEST_EQ(lround(std::numeric_limits<Dec>::infinity() * Dec(dist(rng))), LONG_MIN);
    BOOST_TEST_EQ(lround(std::numeric_limits<Dec>::quiet_NaN() * Dec(dist(rng))), LONG_MIN);
    BOOST_TEST_EQ(lround(Dec(0) * Dec(dist(rng))), 0);
    BOOST_TEST_EQ(lround(Dec(0) * Dec(dist(rng)) + Dec(1, -20)), 0);
    BOOST_TEST_EQ(lround(Dec(0) * Dec(dist(rng)) + Dec(1U, -20, true)), 0);
}

template <typename Dec>
void test_llround()
{
    std::uniform_real_distribution<float> dist(-1e20F, 1e20F);

    constexpr auto max_iter {std::is_same<Dec, decimal128_t>::value || std::is_same<Dec, decimal_fast128_t>::value ? N / 4 : N};

    std::uniform_real_distribution<float> dist2(-1e5F, 1e5F);

    for (std::size_t n {}; n < max_iter; ++n)
    {
        const auto val1 {dist2(rng)};
        Dec d1 {val1};

        auto ret_val {std::llround(val1)};
        auto ret_dec {llround(d1)};

        // Difference in significant figures
        float iptr {};
        const auto frac {std::modf(val1, &iptr)};
        if (std::abs(frac) <= 0.5F && std::abs(frac) >= 0.49F)
        {
            continue;
        }

        if (!BOOST_TEST_EQ(ret_val, ret_dec))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << d1
                      << "\nRet val: " << ret_val
                      << "\nRet dec: " << ret_dec << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    BOOST_TEST_EQ(llround(std::numeric_limits<Dec>::infinity() * Dec(dist(rng))), LLONG_MIN);
    BOOST_TEST_EQ(llround(std::numeric_limits<Dec>::quiet_NaN() * Dec(dist(rng))), LLONG_MIN);
    BOOST_TEST_EQ(llround(Dec(0) * Dec(dist(rng))), 0);
    BOOST_TEST_EQ(llround(Dec(0) * Dec(dist(rng)) + Dec(1, -20)), 0);
    BOOST_TEST_EQ(llround(Dec(0) * Dec(dist(rng)) + Dec(1U, -20, true)), 0);
}

template <typename Dec>
void test_nextafter()
{
    std::uniform_real_distribution<float> dist(1.0F, 1e5F);

    constexpr auto max_iter {std::is_same<Dec, decimal128_t>::value || std::is_same<Dec, decimal_fast128_t>::value ? N / 4 : N};
    for (std::size_t n {}; n < max_iter; ++n)
    {
        const auto val1 {dist(rng)};
        const auto val2 {dist(rng)};
        Dec d1 {val1};
        Dec d2 {val2};

        auto ret_val {std::nextafter(val1, val2)};
        auto ret_dec {static_cast<float>(nextafter(d1, d2))};

        if (ret_val == 0 || ret_dec == 0)
        {
            BOOST_TEST_EQ(ret_val, ret_dec); // LCOV_EXCL_LINE
        }
        else if (!BOOST_TEST(boost::math::float_distance(ret_val, ret_dec) < 25))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << d1
                      << "\nVal 2: " << val2
                      << "\nDec 2: " << d2
                      << "\nRet val: " << ret_val
                      << "\nRet dec: " << ret_dec
                      << "\nDist: " << boost::math::float_distance(ret_val, ret_dec) << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    BOOST_TEST(isinf(nextafter(std::numeric_limits<Dec>::infinity() * Dec(dist(rng)), Dec(1))));
    BOOST_TEST(isnan(nextafter(std::numeric_limits<Dec>::quiet_NaN() * Dec(dist(rng)), Dec(1))));
    BOOST_TEST(isnan(nextafter(Dec(1), std::numeric_limits<Dec>::quiet_NaN() * Dec(dist(rng)))));
    BOOST_TEST(!isinf(nextafter(Dec(1), std::numeric_limits<Dec>::infinity() * Dec(dist(rng)))));
    BOOST_TEST_EQ(nextafter(Dec(1), Dec(1)), Dec(1));
    BOOST_TEST_EQ(nextafter(Dec(0), Dec(1)), std::numeric_limits<Dec>::denorm_min());
    BOOST_TEST_EQ(nextafter(Dec(0), Dec(-1)), -std::numeric_limits<Dec>::denorm_min());
}

template <typename Dec>
void test_nexttoward()
{
    std::uniform_real_distribution<float> dist(1.0F, 1e5F);

    constexpr auto max_iter {std::is_same<Dec, decimal128_t>::value || std::is_same<Dec, decimal_fast128_t>::value ? N / 4 : N};
    for (std::size_t n {}; n < max_iter; ++n)
    {
        const auto val1 {dist(rng)};
        const auto val2 {dist(rng)};
        Dec d1 {val1};
        Dec d2 {val2};

        auto ret_val {std::nexttoward(val1, val2)};
        auto ret_dec {static_cast<float>(nexttoward(d1, val2))};

        if (ret_val == 0 || ret_dec == 0)
        {
            BOOST_TEST_EQ(ret_val, ret_dec); // LCOV_EXCL_LINE
        }
        else if (!BOOST_TEST(boost::math::float_distance(ret_val, ret_dec) < 25))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << d1
                      << "\nVal 2: " << val2
                      << "\nDec 2: " << d2
                      << "\nRet val: " << ret_val
                      << "\nRet dec: " << ret_dec
                      << "\nDist: " << boost::math::float_distance(ret_val, ret_dec) << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    BOOST_TEST(isinf(nexttoward(std::numeric_limits<Dec>::infinity() * Dec(dist(rng)), 1)));
    BOOST_TEST(isnan(nexttoward(std::numeric_limits<Dec>::quiet_NaN() * Dec(dist(rng)), 1)));
    BOOST_TEST_EQ(nexttoward(Dec(1), 1), Dec(1));
    BOOST_TEST_EQ(nexttoward(Dec(0), 1), std::numeric_limits<Dec>::denorm_min());
    BOOST_TEST_EQ(nexttoward(Dec(0), -1), -std::numeric_limits<Dec>::denorm_min());
}

template <typename T>
void test_pow()
{
    std::uniform_int_distribution<int> dist(1, 1);

    const T two {2, 0};
    BOOST_TEST_EQ(pow(two, dist(rng)), two);
    BOOST_TEST_EQ(pow(two, 2 * dist(rng)), two * two);
    BOOST_TEST_EQ(pow(two, 3 * dist(rng)), two * two * two);
    BOOST_TEST_EQ(pow(two, 4 * dist(rng)), two * two * two * two);
}

template <typename T>
void test_exp2()
{
    std::uniform_int_distribution<int> dist(1, 1);

    const T two {2, 0};
    BOOST_TEST_EQ(exp2(two), two * two * dist(rng));
    BOOST_TEST_EQ(exp2(T(0 * dist(rng))), T(dist(rng)));
    BOOST_TEST(isnan(exp2(std::numeric_limits<T>::quiet_NaN())));
    BOOST_TEST(isinf(exp2(std::numeric_limits<T>::infinity())));
    BOOST_TEST_EQ(exp2(-std::numeric_limits<T>::infinity()), T(0 * dist(rng)));
}

#if !defined(BOOST_DECIMAL_DISABLE_CLIB)

#if defined(__GNUC__) && __GNUC__ >= 8
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif

template <typename T>
auto test_nan()
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_ieee_type_v, T, void)
{
    using sig_type = typename T::significand_type;

    const std::array<sig_type, 5> sigs {1U, 2U, 3U, 0U, 0U};
    constexpr std::array<const char*, 5> payloads {"1", "2", "3", "Junk", "999999999999999999999999999999999999999999999999999999999999"};

    const T quiet_nan {std::numeric_limits<T>::quiet_NaN()};
    sig_type quiet_nan_bits;
    std::memcpy(&quiet_nan_bits, &quiet_nan, sizeof(sig_type));

    const T signaling_nan {std::numeric_limits<T>::signaling_NaN()};
    sig_type signaling_nan_bits;
    std::memcpy(&signaling_nan_bits, &signaling_nan, sizeof(sig_type));

    for (std::size_t i {}; i < sigs.size(); ++i)
    {
        const auto payload {nan<T>(payloads[i])};
        BOOST_TEST(isnan(payload));
        BOOST_TEST(!issignaling(payload));

        // Check the payload
        sig_type bits {};
        std::memcpy(&bits, &payload, sizeof(sig_type));
        bits ^= quiet_nan_bits;
        BOOST_TEST_EQ(bits, sigs[i]);

        const auto payload_func_bits {read_payload(payload)};
        BOOST_TEST_EQ(payload_func_bits, bits);
    }

    for (std::size_t i {}; i < sigs.size(); ++i)
    {
        const auto payload {snan<T>(payloads[i])};
        BOOST_TEST(isnan(payload));
        BOOST_TEST(issignaling(payload));

        // Check the payload
        sig_type bits {};
        std::memcpy(&bits, &payload, sizeof(sig_type));
        bits ^= signaling_nan_bits;
        BOOST_TEST_EQ(bits, sigs[i]);

        const auto payload_func_bits {read_payload(payload)};
        BOOST_TEST_EQ(payload_func_bits, bits);
    }
}

template <typename T>
auto test_nan()
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_fast_type_v, T, void)
{
    using sig_type = typename T::significand_type;

    const std::array<sig_type, 5> sigs {1U, 2U, 3U, 0U, 0U};
    constexpr std::array<const char*, 5> payloads {"1", "2", "3", "Junk", "999999999999999999999999999999999999999999999999999999999999"};

    for (std::size_t i {}; i < sigs.size(); ++i)
    {
        const auto payload {nan<T>(payloads[i])};
        BOOST_TEST(isnan(payload));
        BOOST_TEST(!issignaling(payload));

        const auto recovered_payload {read_payload(payload)};
        BOOST_TEST_EQ(recovered_payload, sigs[i]);
    }

    for (std::size_t i {}; i < sigs.size(); ++i)
    {
        const auto payload {snan<T>(payloads[i])};
        BOOST_TEST(isnan(payload));
        BOOST_TEST(issignaling(payload));

        const auto recovered_payload {read_payload(payload)};
        BOOST_TEST_EQ(recovered_payload, sigs[i]);
    }
}

#if defined(__GNUC__) && __GNUC__ >= 8
#  pragma GCC diagnostic pop
#endif

#endif

template <typename Dec>
void test_log2()
{
    std::uniform_real_distribution<float> dist(-0.5F, 0.5F);

    constexpr auto max_iter {std::is_same<Dec, decimal128_t>::value || std::is_same<Dec, decimal_fast128_t>::value ? N / 4 : N};
    for (std::size_t n {}; n < max_iter; ++n)
    {
        const auto val1 {dist(rng)};
        Dec d1 {val1};

        auto ret_val {std::log2(val1)};
        auto ret_dec {static_cast<float>(log2(d1))};

        if (!std::isfinite(ret_val) && !std::isfinite(ret_dec))
        {
            continue;
        }

        const auto distance {std::fabs(boost::math::float_distance(ret_val, ret_dec))};
        if (!BOOST_TEST(distance < 100))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << d1
                      << "\nRet val: " << ret_val
                      << "\nRet dec: " << ret_dec
                      << "\nDist: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }
}

template <typename Dec>
void test_log10()
{
    std::uniform_real_distribution<float> dist(-0.5F, 0.5F);

    constexpr auto max_iter {std::is_same<Dec, decimal128_t>::value || std::is_same<Dec, decimal_fast128_t>::value ? N / 4 : N};
    for (std::size_t n {}; n < max_iter; ++n)
    {
        const auto val1 {dist(rng)};
        Dec d1 {val1};

        auto ret_val {std::log10(val1)};
        auto ret_dec {static_cast<float>(log10(d1))};

        if (!std::isfinite(ret_val) && !std::isfinite(ret_dec))
        {
            continue;
        }

        const auto distance {std::fabs(boost::math::float_distance(ret_val, ret_dec))};
        if (!BOOST_TEST(distance < 100))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << d1
                      << "\nRet val: " << ret_val
                      << "\nRet dec: " << ret_dec
                      << "\nDist: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }
}

template <typename T>
void test_fpclassify()
{
    std::uniform_int_distribution<int> dist(1, 2);
    BOOST_TEST_EQ(fpclassify(T(1) * dist(rng)), FP_NORMAL);
    BOOST_TEST_EQ(fpclassify(std::numeric_limits<T>::infinity() * dist(rng)), FP_INFINITE);
    BOOST_TEST_EQ(fpclassify(std::numeric_limits<T>::quiet_NaN() * dist(rng)), FP_NAN);
    BOOST_TEST_EQ(fpclassify(T{0} * dist(rng)), FP_ZERO);
    BOOST_TEST_EQ(fpclassify(-T{0} * dist(rng)), FP_ZERO);
}

int main()
{
    test_fmax<decimal32_t>();
    test_isgreater<decimal32_t>();
    test_isgreaterequal<decimal32_t>();
    test_fmin<decimal32_t>();
    test_isless<decimal32_t>();
    test_islessequal<decimal32_t>();
    test_islessgreater<decimal32_t>();
    test_isunordered<decimal32_t>();

    test_fmax<decimal_fast32_t>();
    test_isgreater<decimal_fast32_t>();
    test_isgreaterequal<decimal_fast32_t>();
    test_fmin<decimal_fast32_t>();
    test_isless<decimal_fast32_t>();
    test_islessequal<decimal_fast32_t>();
    test_islessgreater<decimal_fast32_t>();
    test_isunordered<decimal_fast32_t>();

    test_fmax<decimal64_t>();
    test_isgreater<decimal64_t>();
    test_isgreaterequal<decimal64_t>();
    test_fmin<decimal64_t>();
    test_isless<decimal64_t>();
    test_islessequal<decimal64_t>();
    test_islessgreater<decimal64_t>();
    test_isunordered<decimal64_t>();

    test_fmax<decimal_fast64_t>();
    test_isgreater<decimal_fast64_t>();
    test_isgreaterequal<decimal_fast64_t>();
    test_fmin<decimal_fast64_t>();
    test_isless<decimal_fast64_t>();
    test_islessequal<decimal_fast64_t>();
    test_islessgreater<decimal_fast64_t>();
    test_isunordered<decimal_fast64_t>();

    test_fmax<decimal128_t>();
    test_isgreater<decimal128_t>();
    test_isgreaterequal<decimal128_t>();
    test_fmin<decimal128_t>();
    test_isless<decimal128_t>();
    test_islessequal<decimal128_t>();
    test_islessgreater<decimal128_t>();
    test_isunordered<decimal128_t>();

    test_fmax<decimal_fast128_t>();
    test_isgreater<decimal_fast128_t>();
    test_isgreaterequal<decimal_fast128_t>();
    test_fmin<decimal_fast128_t>();
    test_isless<decimal_fast128_t>();
    test_islessequal<decimal_fast128_t>();
    test_islessgreater<decimal_fast128_t>();
    test_isunordered<decimal_fast128_t>();

    test_floor<decimal32_t>();
    test_ceil<decimal32_t>();
    test_trunc<decimal32_t>();

    test_floor<decimal_fast32_t>();
    test_ceil<decimal_fast32_t>();
    test_trunc<decimal_fast32_t>();

    test_floor<decimal64_t>();
    test_ceil<decimal64_t>();
    test_trunc<decimal64_t>();

    test_floor<decimal_fast64_t>();
    test_ceil<decimal_fast64_t>();
    test_trunc<decimal_fast64_t>();

    test_floor<decimal128_t>();
    test_ceil<decimal128_t>();
    test_trunc<decimal128_t>();

    test_floor<decimal_fast128_t>();
    test_ceil<decimal_fast128_t>();
    test_trunc<decimal_fast128_t>();

    test_frexp10<decimal32_t>();
    test_scalbn<decimal32_t>();
    test_scalbln<decimal32_t>();

    test_frexp10<decimal_fast32_t>();
    test_scalbn<decimal_fast32_t>();
    test_scalbln<decimal_fast32_t>();

    test_frexp10<decimal64_t>();
    test_scalbn<decimal64_t>();
    test_scalbln<decimal64_t>();

    test_frexp10<decimal_fast64_t>();
    test_scalbn<decimal_fast64_t>();
    test_scalbln<decimal_fast64_t>();

    test_div_fmod<decimal32_t>();
    test_div_fmod<decimal64_t>();

    test_copysign<decimal32_t>();
    test_copysign<decimal64_t>();

    test_fma<decimal32_t>();
    test_fma<decimal_fast32_t>();
    test_fma<decimal64_t>();
    test_fma<decimal_fast64_t>();
    test_fma<decimal128_t>();
    test_fma<decimal_fast128_t>();

    test_modf<decimal32_t>();
    test_modf<decimal_fast32_t>();
    test_modf<decimal64_t>();
    test_modf<decimal_fast64_t>();

    test_fdim<decimal32_t>();
    test_fdim<decimal_fast32_t>();
    test_fdim<decimal64_t>();
    test_fdim<decimal_fast64_t>();

    test_ilogb<decimal32_t>();
    test_ilogb<decimal_fast32_t>();
    test_ilogb<decimal64_t>();
    test_ilogb<decimal_fast64_t>();
    test_ilogb<decimal128_t>();
    test_ilogb<decimal_fast128_t>();

    test_logb<decimal32_t>();
    test_logb<decimal_fast32_t>();
    test_logb<decimal64_t>();
    test_logb<decimal_fast64_t>();
    test_logb<decimal128_t>();
    test_logb<decimal_fast128_t>();

    test_sqrt<decimal32_t>();
    test_sqrt<decimal_fast32_t>();
    test_sqrt<decimal64_t>();
    test_sqrt<decimal_fast64_t>();

    test_two_val_hypot<decimal32_t>();
    test_three_val_hypot<decimal32_t>();
    test_two_val_hypot<decimal_fast32_t>();
    test_three_val_hypot<decimal_fast32_t>();
    test_two_val_hypot<decimal64_t>();
    test_three_val_hypot<decimal64_t>();
    test_two_val_hypot<decimal_fast64_t>();
    test_three_val_hypot<decimal_fast64_t>();

    #if !defined(BOOST_DECIMAL_REDUCE_TEST_DEPTH)
    test_sqrt<decimal128_t>();
    test_sqrt<decimal_fast128_t>();

    test_two_val_hypot<decimal128_t>();
    test_three_val_hypot<decimal128_t>();
    test_mixed_two_val_hypot<decimal64_t, decimal128_t>();

    test_two_val_hypot<decimal_fast128_t>();
    test_three_val_hypot<decimal_fast128_t>();
    test_mixed_two_val_hypot<decimal64_t, decimal_fast128_t>();
    #endif

    test_mixed_two_val_hypot<decimal32_t, decimal64_t>();

    test_rint<decimal32_t>();
    test_lrint<decimal32_t>();
    test_llrint<decimal32_t>();
    test_nearbyint<decimal32_t>();

    test_rint<decimal_fast32_t>();
    test_lrint<decimal_fast32_t>();
    test_llrint<decimal_fast32_t>();
    test_nearbyint<decimal_fast32_t>();

    test_rint<decimal64_t>();
    test_lrint<decimal64_t>();
    test_llrint<decimal64_t>();
    test_nearbyint<decimal64_t>();

    test_rint<decimal_fast64_t>();
    test_lrint<decimal_fast64_t>();
    test_llrint<decimal_fast64_t>();
    test_nearbyint<decimal_fast64_t>();

    test_round<decimal32_t>();
    test_lround<decimal32_t>();
    test_llround<decimal32_t>();

    test_round<decimal_fast32_t>();
    test_lround<decimal_fast32_t>();
    test_llround<decimal_fast32_t>();

    test_round<decimal64_t>();
    test_lround<decimal64_t>();
    test_llround<decimal64_t>();

    test_round<decimal_fast64_t>();
    test_lround<decimal_fast64_t>();
    test_llround<decimal_fast64_t>();

    test_nextafter<decimal32_t>();
    test_nextafter<decimal_fast32_t>();
    test_nextafter<decimal64_t>();
    test_nextafter<decimal_fast64_t>();

    #ifndef BOOST_DECIMAL_UNSUPPORTED_LONG_DOUBLE
    test_nexttoward<decimal32_t>();
    test_nexttoward<decimal_fast32_t>();
    test_nexttoward<decimal64_t>();
    test_nexttoward<decimal_fast64_t>();
    #endif

    test_pow<decimal32_t>();
    test_pow<decimal_fast32_t>();
    test_pow<decimal64_t>();
    test_pow<decimal_fast64_t>();

    test_exp2<decimal32_t>();
    test_exp2<decimal_fast32_t>();
    test_exp2<decimal64_t>();

    #if !defined(BOOST_DECIMAL_DISABLE_CLIB)
    test_nan<decimal32_t>();
    test_nan<decimal64_t>();
    test_nan<decimal128_t>();

    test_nan<decimal_fast32_t>();
    test_nan<decimal_fast64_t>();
    test_nan<decimal_fast128_t>();
    #endif

    test_log2<decimal32_t>();
    test_log2<decimal_fast32_t>();
    test_log2<decimal64_t>();
    test_log2<decimal_fast64_t>();

    test_log10<decimal32_t>();
    test_log10<decimal_fast32_t>();
    test_log10<decimal64_t>();
    test_log10<decimal_fast64_t>();

    #if !defined(BOOST_DECIMAL_REDUCE_TEST_DEPTH)
    test_log2<decimal128_t>();
    test_log10<decimal128_t>();

    test_log2<decimal_fast128_t>();
    test_log10<decimal_fast128_t>();
    #endif

    test_fpclassify<decimal32_t>();
    test_fpclassify<decimal64_t>();
    test_fpclassify<decimal128_t>();

    test_fpclassify<decimal_fast32_t>();
    test_fpclassify<decimal_fast64_t>();
    test_fpclassify<decimal_fast128_t>();

    return boost::report_errors();
}
