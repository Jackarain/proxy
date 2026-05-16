// Copyright 2024 Matt Borland
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

#include <boost/math/special_functions/next.hpp>
#include <boost/core/lightweight_test.hpp>


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
void test_acos()
{
    constexpr auto max_iter {std::is_same<Dec, decimal128_t>::value ? N / 4 : N};

    for (std::size_t n {}; n < max_iter; ++n)
    {
        std::uniform_real_distribution<float> range_1(-0.9999F, -0.5F);
        const auto val1 {range_1(rng)};
        Dec d1 {val1};

        auto ret_val {std::acos(val1)};
        auto ret_dec {static_cast<float>(acos(d1))};

        const auto distance {std::fabs(boost::math::float_distance(ret_val, ret_dec))};
        if (!BOOST_TEST(distance < 50))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << d1
                      << "\nRet val: " << ret_val
                      << "\nRet dec: " << ret_dec
                      << "\nEps: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    for (std::size_t n {}; n < max_iter; ++n)
    {
        std::uniform_real_distribution<float> range_2(-0.5F, -static_cast<float>(std::numeric_limits<Dec>::epsilon()));
        const auto val1 {range_2(rng)};
        Dec d1 {val1};

        auto ret_val {std::acos(val1)};
        auto ret_dec {static_cast<float>(acos(d1))};

        const auto distance {std::fabs(boost::math::float_distance(ret_val, ret_dec))};
        if (!BOOST_TEST(distance < 50))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << d1
                      << "\nRet val: " << ret_val
                      << "\nRet dec: " << ret_dec
                      << "\nEps: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    for (std::size_t n {}; n < max_iter; ++n)
    {
        std::uniform_real_distribution<float> range_3(-static_cast<float>(std::numeric_limits<Dec>::epsilon()), static_cast<float>(std::numeric_limits<Dec>::epsilon()));
        const auto val1 {range_3(rng)};
        Dec d1 {val1};

        auto ret_val {std::acos(val1)};
        auto ret_dec {static_cast<float>(acos(d1))};

        const auto distance {std::fabs(boost::math::float_distance(ret_val, ret_dec))};
        if (!BOOST_TEST(distance < 50))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << d1
                      << "\nRet val: " << ret_val
                      << "\nRet dec: " << ret_dec
                      << "\nEps: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    for (std::size_t n {}; n < max_iter; ++n)
    {
        std::uniform_real_distribution<float> range_4( static_cast<float>(std::numeric_limits<Dec>::epsilon()), 0.5F);
        const auto val1 {range_4(rng)};
        Dec d1 {val1};

        auto ret_val {std::acos(val1)};
        auto ret_dec {static_cast<float>(acos(d1))};

        const auto distance {std::fabs(boost::math::float_distance(ret_val, ret_dec))};
        if (!BOOST_TEST(distance < 50))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << d1
                      << "\nRet val: " << ret_val
                      << "\nRet dec: " << ret_dec
                      << "\nEps: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    for (std::size_t n {}; n < max_iter; ++n)
    {
        std::uniform_real_distribution<float> range_5( 0.5F, 0.9999F);
        const auto val1 {range_5(rng)};
        Dec d1 {val1};

        auto ret_val {std::acos(val1)};
        auto ret_dec {static_cast<float>(acos(d1))};

        const auto distance {std::fabs(boost::math::float_distance(ret_val, ret_dec))};
        if (!BOOST_TEST(distance < 400))
        {
            // LCOV_EXCL_START
            std::cerr << std::setprecision(std::numeric_limits<Dec>::digits10)
                      << "Val 1: " << val1
                      << "\nDec 1: " << d1
                      << "\nRet val: " << ret_val
                      << "\nRet dec: " << ret_dec
                      << "\nEps: " << distance
                      << "\nLoop #: " << n << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    // Edge cases
    std::uniform_int_distribution<int> one(1,1);
    BOOST_TEST(isnan(acos(std::numeric_limits<Dec>::infinity() * Dec(one(rng)))));
    BOOST_TEST(isnan(acos(-std::numeric_limits<Dec>::infinity() * Dec(one(rng)))));
    BOOST_TEST(isnan(acos(std::numeric_limits<Dec>::quiet_NaN() * Dec(one(rng))))); 
}

int main()
{
    test_acos<decimal32_t>();
    test_acos<decimal64_t>();

    test_acos<decimal_fast32_t>();

    return boost::report_errors();
}
