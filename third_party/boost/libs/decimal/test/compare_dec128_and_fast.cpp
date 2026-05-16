// Copyright 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include "testing_config.hpp"
#include <boost/decimal.hpp>
#include <random>
#include <limits>
#include <climits>
#include <iostream>
#include <iomanip>
#include <sstream>

#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wfloat-equal"
#elif defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#endif

#include <boost/core/lightweight_test.hpp>

using namespace boost::decimal;

#if !defined(BOOST_DECIMAL_REDUCE_TEST_DEPTH)
static constexpr auto N = static_cast<std::size_t>(128); // Number of trials
#else
static constexpr auto N = static_cast<std::size_t>(8); // Number of trials
#endif

static std::mt19937_64 rng(42);

#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable: 4146)
#endif

#if defined(__GNUC__) && __GNUC__ >= 8
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif

void test_add()
{
    std::uniform_real_distribution<double> big_vals(std::numeric_limits<double>::lowest(), std::numeric_limits<double>::max());

    for (std::size_t i {}; i < N; ++i)
    {
        const auto val1 {big_vals(rng)};
        const auto val2 {big_vals(rng)};

        const decimal128_t dec128_1 {val1};
        const decimal128_t dec128_2 {val2};
        const decimal128_t dec128_res {dec128_1 + dec128_2};

        const decimal_fast128_t dec128_fast_1 {val1};
        const decimal_fast128_t dec128_fast_2 {val2};
        const decimal_fast128_t dec128_fast_res {dec128_fast_1 + dec128_fast_2};

        if (!BOOST_TEST_EQ(static_cast<double>(dec128_res), static_cast<double>(dec128_fast_res)))
        {
            // LCOV_EXCL_START
            std::stringstream strm;

            strm << std::setprecision(35)
                 <<   "Val 1: " << val1
                 << "\nVal 2: " << val2
                 << "\nDec 1: " << dec128_1
                 << "\nDec 2: " << dec128_2
                 << "\nDec Res: " << dec128_res
                 << "\nDecfast 1: " << dec128_fast_1
                 << "\nDecfast 2: " << dec128_fast_2
                 << "\nDecfast res: " << dec128_fast_res;

            std::cerr << strm.str() << std::endl;
            // LCOV_EXCL_STOP
        } // LCOV_EXCL_LINE
    }

    std::uniform_real_distribution<double> small_vals(0.0, 1.0);

    for (std::size_t i {}; i < N; ++i)
    {
        const auto val1 {small_vals(rng)};
        const auto val2 {small_vals(rng)};

        const decimal128_t dec128_1 {val1};
        const decimal128_t dec128_2 {val2};
        const decimal128_t dec128_res {dec128_1 + dec128_2};

        const decimal_fast128_t dec128_fast_1 {val1};
        const decimal_fast128_t dec128_fast_2 {val2};
        const decimal_fast128_t dec128_fast_res {dec128_fast_1 + dec128_fast_2};

        if (!BOOST_TEST_EQ(static_cast<double>(dec128_res), static_cast<double>(dec128_fast_res)))
        {
            // LCOV_EXCL_START
            std::stringstream strm;

            strm << std::setprecision(35)
                 <<   "Val 1: " << val1
                 << "\nVal 2: " << val2
                 << "\nDec 1: " << dec128_1
                 << "\nDec 2: " << dec128_2
                 << "\nDec Res: " << dec128_res
                 << "\nDecfast 1: " << dec128_fast_1
                 << "\nDecfast 2: " << dec128_fast_2
                 << "\nDecfast res: " << dec128_fast_res;

            std::cerr << strm.str() << std::endl;
            // LCOV_EXCL_STOP
        } // LCOV_EXCL_LINE
    }
}

void test_sub()
{
    std::uniform_real_distribution<double> big_vals(std::numeric_limits<double>::lowest(), std::numeric_limits<double>::max());

    for (std::size_t i {}; i < N; ++i)
    {
        const auto val1 {big_vals(rng)};
        const auto val2 {big_vals(rng)};

        const decimal128_t dec128_1 {val1};
        const decimal128_t dec128_2 {val2};
        const decimal128_t dec128_res {dec128_1 - dec128_2};

        const decimal_fast128_t dec128_fast_1 {val1};
        const decimal_fast128_t dec128_fast_2 {val2};
        const decimal_fast128_t dec128_fast_res {dec128_fast_1 + dec128_fast_2};

        if (isinf(dec128_1) && isinf(dec128_2) && isinf(dec128_fast_1) && isinf(dec128_fast_2))
        {
            BOOST_TEST(isinf(dec128_res) || isnan(dec128_res));
            BOOST_TEST(isinf(dec128_fast_res) || isnan(dec128_fast_res));
            continue;
        }

        if (!BOOST_TEST_EQ(static_cast<double>(dec128_res), static_cast<double>(dec128_fast_res)))
        {
            // LCOV_EXCL_START
            std::stringstream strm;

            strm << std::setprecision(35)
                 <<   "Val 1: " << val1
                 << "\nVal 2: " << val2
                 << "\nDec 1: " << dec128_1
                 << "\nDec 2: " << dec128_2
                 << "\nDec Res: " << dec128_res
                 << "\nDecfast 1: " << dec128_fast_1
                 << "\nDecfast 2: " << dec128_fast_2
                 << "\nDecfast res: " << dec128_fast_res;

            std::cerr << strm.str() << std::endl;
            // LCOV_EXCL_STOP
        } // LCOV_EXCL_LINE
    }

    std::uniform_real_distribution<double> small_vals(0.0, 1.0);

    for (std::size_t i {}; i < N; ++i)
    {
        const auto val1 {small_vals(rng)};
        const auto val2 {small_vals(rng)};

        const decimal128_t dec128_1 {val1};
        const decimal128_t dec128_2 {val2};
        const decimal128_t dec128_res {dec128_1 - dec128_2};

        const decimal_fast128_t dec128_fast_1 {val1};
        const decimal_fast128_t dec128_fast_2 {val2};
        const decimal_fast128_t dec128_fast_res {dec128_fast_1 - dec128_fast_2};

        if (!BOOST_TEST_EQ(static_cast<double>(dec128_res), static_cast<double>(dec128_fast_res)))
        {
            // LCOV_EXCL_START
            std::stringstream strm;

            strm << std::setprecision(35)
                 <<   "Val 1: " << val1
                 << "\nVal 2: " << val2
                 << "\nDec 1: " << dec128_1
                 << "\nDec 2: " << dec128_2
                 << "\nDec Res: " << dec128_res
                 << "\nDecfast 1: " << dec128_fast_1
                 << "\nDecfast 2: " << dec128_fast_2
                 << "\nDecfast res: " << dec128_fast_res;

            std::cerr << strm.str() << std::endl;
            // LCOV_EXCL_STOP
        } // LCOV_EXCL_LINE
    }
}

void test_mul()
{
    std::uniform_real_distribution<double> big_vals(std::numeric_limits<double>::lowest(), std::numeric_limits<double>::max());

    for (std::size_t i {}; i < N; ++i)
    {
        const auto val1 {big_vals(rng)};
        const auto val2 {big_vals(rng)};

        const decimal128_t dec128_1 {val1};
        const decimal128_t dec128_2 {val2};
        const decimal128_t dec128_res {dec128_1 * dec128_2};

        const decimal_fast128_t dec128_fast_1 {val1};
        const decimal_fast128_t dec128_fast_2 {val2};
        const decimal_fast128_t dec128_fast_res {dec128_fast_1 * dec128_fast_2};

        if (!BOOST_TEST_EQ(static_cast<double>(dec128_res), static_cast<double>(dec128_fast_res)))
        {
            // LCOV_EXCL_START

            std::cerr << std::setprecision(35)
                      << "Val 1: " << val1
                      << "\nVal 2: " << val2
                      << "\nDec 1: " << dec128_1
                      << "\nDec 2: " << dec128_2
                      << "\nDec Res: " << dec128_res
                      << "\nDecfast 1: " << dec128_fast_1
                      << "\nDecfast 2: " << dec128_fast_2
                      << "\nDecfast res: " << dec128_fast_res << std::endl;

            // LCOV_EXCL_STOP
        }
    }

    std::uniform_real_distribution<double> small_vals(0.0, 1.0);

    for (std::size_t i {}; i < N; ++i)
    {
        const auto val1 {small_vals(rng)};
        const auto val2 {small_vals(rng)};

        const decimal128_t dec128_1 {val1};
        const decimal128_t dec128_2 {val2};
        const decimal128_t dec128_res {dec128_1 * dec128_2};

        const decimal_fast128_t dec128_fast_1 {val1};
        const decimal_fast128_t dec128_fast_2 {val2};
        const decimal_fast128_t dec128_fast_res {dec128_fast_1 * dec128_fast_2};

        if (!BOOST_TEST_EQ(static_cast<double>(dec128_res), static_cast<double>(dec128_fast_res)))
        {
            // LCOV_EXCL_START

            std::cerr << std::setprecision(35)
                      << "Val 1: " << val1
                      << "\nVal 2: " << val2
                      << "\nDec 1: " << dec128_1
                      << "\nDec 2: " << dec128_2
                      << "\nDec Res: " << dec128_res
                      << "\nDecfast 1: " << dec128_fast_1
                      << "\nDecfast 2: " << dec128_fast_2
                      << "\nDecfast res: " << dec128_fast_res << std::endl;

            // LCOV_EXCL_STOP
        }
    }
}

void test_div()
{
    std::uniform_real_distribution<double> big_vals(std::numeric_limits<double>::lowest(), std::numeric_limits<double>::max());

    for (std::size_t i {}; i < N; ++i)
    {
        const auto val1 {big_vals(rng)};
        const auto val2 {big_vals(rng)};

        const decimal128_t dec128_1 {val1};
        const decimal128_t dec128_2 {val2};
        const decimal128_t dec128_res {dec128_1 / dec128_2};

        const decimal_fast128_t dec128_fast_1 {val1};
        const decimal_fast128_t dec128_fast_2 {val2};
        const decimal_fast128_t dec128_fast_res {dec128_fast_1 / dec128_fast_2};

        if (isinf(dec128_1) && isinf(dec128_2) && isinf(dec128_fast_1) && isinf(dec128_fast_2))
        {
            BOOST_TEST(isnan(dec128_res) && isnan(dec128_fast_res));
            continue;
        }

        if (!BOOST_TEST_EQ(static_cast<double>(dec128_res), static_cast<double>(dec128_fast_res)))
        {
            // LCOV_EXCL_START

            std::cerr << std::setprecision(35)
                      << "Val 1: " << val1
                      << "\nVal 2: " << val2
                      << "\nDec 1: " << dec128_1
                      << "\nDec 2: " << dec128_2
                      << "\nDec Res: " << dec128_res
                      << "\nDecfast 1: " << dec128_fast_1
                      << "\nDecfast 2: " << dec128_fast_2
                      << "\nDecfast res: " << dec128_fast_res << std::endl;

            // LCOV_EXCL_STOP
        }
    }

    std::uniform_real_distribution<double> small_vals(0.0, 1.0);

    for (std::size_t i {}; i < N; ++i)
    {
        const auto val1 {small_vals(rng)};
        const auto val2 {small_vals(rng)};

        const decimal128_t dec128_1 {val1};
        const decimal128_t dec128_2 {val2};
        const decimal128_t dec128_res {dec128_1 / dec128_2};

        const decimal_fast128_t dec128_fast_1 {val1};
        const decimal_fast128_t dec128_fast_2 {val2};
        const decimal_fast128_t dec128_fast_res {dec128_fast_1 / dec128_fast_2};

        if (!BOOST_TEST_EQ(static_cast<double>(dec128_res), static_cast<double>(dec128_fast_res)))
        {
            // LCOV_EXCL_START

            std::cerr << std::setprecision(35)
                      << "Val 1: " << val1
                      << "\nVal 2: " << val2
                      << "\nDec 1: " << dec128_1
                      << "\nDec 2: " << dec128_2
                      << "\nDec Res: " << dec128_res
                      << "\nDecfast 1: " << dec128_fast_1
                      << "\nDecfast 2: " << dec128_fast_2
                      << "\nDecfast res: " << dec128_fast_res << std::endl;

            // LCOV_EXCL_STOP
        }
    }
}

int main()
{
    #if !(defined(__i386) || defined(_M_IX86)) && !(defined(_MSVC_LANG))
    test_add();
    test_sub();
    test_mul();
    test_div();
    #endif

    return boost::report_errors();
}
