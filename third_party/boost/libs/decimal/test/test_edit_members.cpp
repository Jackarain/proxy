// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#define BOOST_DECIMAL_DEBUG_MEMBERS

#include "testing_config.hpp"
#include <boost/decimal.hpp>
#include <boost/core/lightweight_test.hpp>
#include <random>
#include <limits>

using namespace boost::decimal;

#if !defined(BOOST_DECIMAL_REDUCE_TEST_DEPTH)
static constexpr auto N = static_cast<std::size_t>(1024); // Number of trials
#else
static constexpr auto N = static_cast<std::size_t>(1024 >> 4U); // Number of trials
#endif

// NOLINTNEXTLINE : Seed with a constant for repeatability
static std::mt19937_64 rng(42); // NOSONAR : Global rng is not const

template <typename T>
void test_edit_exp()
{
    std::uniform_int_distribution<T> sig(1'000'000, 9'999'999);
    std::uniform_int_distribution<T> exp(T(std::numeric_limits<decimal32_t>::min_exponent10 + 19),
                                         T(std::numeric_limits<decimal32_t>::max_exponent10 - 19));

    for (std::size_t i {}; i < N; ++i)
    {
        const auto orig_sig {sig(rng)};
        decimal32_t val {orig_sig, exp(rng)};
        const auto new_exp {exp(rng)};
        const decimal32_t new_val {orig_sig, new_exp};
        val.edit_exponent(new_exp);


        if (!BOOST_TEST_EQ(val, new_val))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val
                      << "\nVal 2: " << new_val << std::endl;
            // LCOV_EXCL_STOP
        }
    }
}

template <typename T>
void test_edit_sig()
{
    std::uniform_int_distribution<T> sig(1'000'000, 9'999'999);
    std::uniform_int_distribution<T> exp(T(std::numeric_limits<decimal32_t>::min_exponent10 + 19),
                                         T(std::numeric_limits<decimal32_t>::max_exponent10 - 19));

    for (std::size_t i {}; i < N; ++i)
    {
        const auto orig_exp {exp(rng)};
        const auto orig_sig {sig(rng)};
        decimal32_t val {orig_sig, orig_exp};
        const auto new_sig {sig(rng)};
        const decimal32_t new_val {new_sig, orig_exp};
        val.edit_significand(new_sig);


        if (!BOOST_TEST_EQ(val, new_val))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val
                      << "\nVal 2: " << new_val << std::endl;
            // LCOV_EXCL_STOP
        }
    }
}

int main()
{
    test_edit_exp<int>();
    test_edit_exp<long>();
    test_edit_exp<long long>();

    test_edit_sig<int>();
    test_edit_sig<long>();
    test_edit_sig<long long>();

    return boost::report_errors();
}
