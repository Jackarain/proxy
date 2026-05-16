// Copyright 2023 - 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

// Propogates up from boost.math
#define _SILENCE_CXX23_DENORM_DEPRECATION_WARNING

#include "testing_config.hpp"
#include <boost/decimal.hpp>

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
#include <type_traits>
#include <iostream>
#include <random>
#include <cmath>

#if !defined(BOOST_DECIMAL_REDUCE_TEST_DEPTH)
static constexpr auto N = static_cast<std::size_t>(128); // Number of trials
#else
static constexpr auto N = static_cast<std::size_t>(128 >> 4U); // Number of trials
#endif

static std::mt19937_64 rng(42);

using namespace boost::decimal;

template <typename Dec>
void test_remainder()
{
    std::uniform_real_distribution<float> dist(-1e3F, 1e3F);

    constexpr auto max_iter {std::is_same<Dec, decimal128_t>::value ? N / 4 : N};
    for (std::size_t n {}; n < max_iter; ++n)
    {
        const auto val1 {dist(rng)};
        const auto val2 {dist(rng)};
        Dec d1 {val1};
        Dec d2 {val2};

        auto ret_val {std::remainder(val1, val2)};
        auto ret_dec {static_cast<float>(remainder(d1, d2))};

        if (!BOOST_TEST(std::fabs(ret_val - ret_dec) < 0.005))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << d1
                      << "\nVal 2: " << val2
                      << "\nDec 2: " << d2
                      << "\nRet val: " << ret_val
                      << "\nRet dec: " << ret_dec
                      << "\nFloat dist: " << boost::math::float_distance(ret_val, ret_dec) << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    BOOST_TEST(isnan(remainder(std::numeric_limits<Dec>::infinity() * Dec(dist(rng)), Dec(1))));
    BOOST_TEST(isnan(remainder(std::numeric_limits<Dec>::quiet_NaN() * Dec(dist(rng)), Dec(1))));
    BOOST_TEST(isnan(remainder(Dec(1), std::numeric_limits<Dec>::quiet_NaN() * Dec(dist(rng)))));
    BOOST_TEST(isnan(remainder(Dec(1), Dec(0))));
}

template <typename Dec>
void test_remquo()
{
    std::uniform_real_distribution<float> dist(-1e3F, 1e3F);

    constexpr auto max_iter {std::is_same<Dec, decimal128_t>::value ? N / 4 : N};
    for (std::size_t n {}; n < max_iter; ++n)
    {
        const auto val1 {dist(rng)};
        const auto val2 {dist(rng)};
        Dec d1 {val1};
        Dec d2 {val2};
        int flt_int {};
        int dec_int {};

        auto ret_val {std::remquo(val1, val2, &flt_int)};
        auto ret_dec {static_cast<float>(remquo(d1, d2, &dec_int))};

        #if defined(__APPLE__) || defined(_MSC_VER)
        if (std::abs(flt_int) > 0b1111)
        #else
        if (std::abs(flt_int) > 0b111)
        #endif
        {
            continue; // LCOV_EXCL_LINE
        }

        if (!(BOOST_TEST(std::fabs(ret_val - ret_dec) < 0.005) && BOOST_TEST(flt_int == dec_int)))
        {
            // LCOV_EXCL_START
            std::cerr << "Test Number: " << n
                      << "\nVal 1: " << val1
                      << "\nDec 1: " << d1
                      << "\nVal 2: " << val2
                      << "\nDec 2: " << d2
                      << "\nRet val: " << ret_val
                      << "\nRet dec: " << ret_dec
                      << "\nInt val: " << flt_int
                      << "\nInt quo: " << val1 / val2
                      << "\nInt dec: " << dec_int
                      << "\nDec quo: " << d1 / d2
                      << "\nFloat dist: " << boost::math::float_distance(ret_val, ret_dec) << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    int quo {};
    BOOST_TEST(isnan(remquo(std::numeric_limits<Dec>::infinity() * Dec(dist(rng)), Dec(1), &quo)));
    BOOST_TEST(isnan(remquo(std::numeric_limits<Dec>::quiet_NaN() * Dec(dist(rng)), Dec(1), &quo)));
    BOOST_TEST(isnan(remquo(Dec(1), std::numeric_limits<Dec>::quiet_NaN() * Dec(dist(rng)), &quo)));
    BOOST_TEST(isnan(remquo(Dec(1), Dec(0), &quo)));
}

int main()
{
    test_remquo<decimal32_t>();
    test_remquo<decimal64_t>();
    test_remquo<decimal128_t>();

    test_remainder<decimal32_t>();
    test_remainder<decimal64_t>();
    test_remainder<decimal128_t>();

    return boost::report_errors();
}
