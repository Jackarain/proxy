// Copyright 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

// Propagates up from boost.math
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
#  pragma GCC diagnostic ignored "-Wfloat-conversion"
#  pragma GCC diagnostic ignored "-Wuseless-cast"
#endif

#include <boost/core/lightweight_test.hpp>
#include <boost/math/special_functions/next.hpp>
#include <boost/math/special_functions/beta.hpp>

#include <cmath>
#include <iostream>
#include <random>

#if !defined(BOOST_DECIMAL_REDUCE_TEST_DEPTH)
static constexpr auto N = static_cast<std::size_t>(128); // Number of trials
#else
static constexpr auto N = static_cast<std::size_t>(128 >> 4U); // Number of trials
#endif

static std::mt19937_64 rng(42);

using namespace boost::decimal;

template <typename T>
void test()
{
    std::uniform_real_distribution<double> dist(0.0, 3.0);

    for (std::size_t n {}; n < N; ++n)
    {
        const auto x {dist(rng)};
        const auto y {dist(rng)};

        const auto double_ret {boost::math::beta(x, y)};

        const auto dec_ret {boost::decimal::beta(static_cast<T>(x), static_cast<T>(y))};
        const auto dec_ret_double {static_cast<double>(dec_ret)};

        if (!BOOST_TEST(std::abs(1.0 - (double_ret / dec_ret_double)) < 1e-5))
        {
            // LCOV_EXCL_START

            std::cerr << std::setprecision(std::numeric_limits<T>::digits10)
                      << "X: " << x
                      << "\nY: " << y
                      << "\nBoost::Math: " << double_ret
                      << "\nDecimal val: " << dec_ret
                      << "\nDist: " << std::abs(double_ret - dec_ret_double) / std::numeric_limits<double>::epsilon() << std::endl;

            // LCOV_EXCL_STOP
        }
    }

    BOOST_TEST(isnan(beta(T(1), T(dist(rng)) * std::numeric_limits<T>::quiet_NaN())));
    BOOST_TEST(isnan(beta(std::numeric_limits<T>::quiet_NaN() * T(dist(rng)), T(1))));
}

int main()
{
    test<decimal32_t>();
    test<decimal_fast32_t>();
    test<decimal64_t>();
    test<decimal_fast64_t>();

    #if !defined(BOOST_DECIMAL_REDUCE_TEST_DEPTH)
    test<decimal128_t>();
    test<decimal_fast128_t>();
    #endif

    return boost::report_errors();
}
