// Copyright 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#define BOOST_DECIMAL_FAST_MATH

#include "testing_config.hpp"
#include <boost/decimal.hpp>
#include <boost/core/lightweight_test.hpp>
#include <random>
#include <cstdint>

using namespace boost::decimal;

#if !defined(BOOST_DECIMAL_REDUCE_TEST_DEPTH)
static constexpr auto N = static_cast<std::size_t>(1024); // Number of trials
#else
static constexpr auto N = static_cast<std::size_t>(1024 >> 4U); // Number of trials
#endif

// NOLINTNEXTLINE : Seed with a constant for repeatability
static std::mt19937_64 rng(42); // NOSONAR : Global rng is not const

template <typename T>
void random_addition()
{
    std::uniform_int_distribution<std::int64_t> dist(1, 1000);

    for (std::size_t i {}; i < N; ++i)
    {
        const T val1 {dist(rng)};
        const T val2 {dist(rng)};

        const T dec1 {val1};
        const T dec2 {val2};

        const T res = dec1 + dec2;
        const auto res_int = static_cast<T>(res);

        if (!BOOST_TEST_EQ(res_int, val1 + val2))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << dec1
                      << "\nVal 2: " << val2
                      << "\nDec 2: " << dec2
                      << "\nDec res: " << res
                      << "\nInt res: " << val1 + val2 << std::endl;
            // LCOV_EXCL_STOP
        }
    }
}

template <typename T>
void random_subtraction()
{
    std::uniform_int_distribution<std::int64_t> dist(1, 1000);

    for (std::size_t i {}; i < N; ++i)
    {
        const T val1 {dist(rng)};
        const T val2 {dist(rng)};

        const T dec1 {val1};
        const T dec2 {val2};

        const T res = dec1 - dec2;
        const auto res_int = static_cast<T>(res);

        if (!BOOST_TEST_EQ(res_int, val1 - val2))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << dec1
                      << "\nVal 2: " << val2
                      << "\nDec 2: " << dec2
                      << "\nDec res: " << res
                      << "\nInt res: " << val1 + val2 << std::endl;
            // LCOV_EXCL_STOP
        }
    }
}

template <typename T>
void random_multiplication()
{
    std::uniform_int_distribution<std::int64_t> dist(1, 1000);

    for (std::size_t i {}; i < N; ++i)
    {
        const T val1 {dist(rng)};
        const T val2 {dist(rng)};

        const T dec1 {val1};
        const T dec2 {val2};

        const T res = dec1 * dec2;
        const auto res_int = static_cast<T>(res);

        if (!BOOST_TEST_EQ(res_int, val1 * val2))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << dec1
                      << "\nVal 2: " << val2
                      << "\nDec 2: " << dec2
                      << "\nDec res: " << res
                      << "\nInt res: " << val1 + val2 << std::endl;
            // LCOV_EXCL_STOP
        }
    }
}

template <typename T>
void random_division()
{
    std::uniform_int_distribution<std::int64_t> dist(1, 1000);

    for (std::size_t i {}; i < N; ++i)
    {
        const T val1 {dist(rng)};
        const T val2 {dist(rng)};

        const T dec1 {val1};
        const T dec2 {val2};

        const T res = dec1 / dec2;
        const auto res_int = static_cast<T>(res);

        if (!BOOST_TEST_EQ(res_int, val1 / val2))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << dec1
                      << "\nVal 2: " << val2
                      << "\nDec 2: " << dec2
                      << "\nDec res: " << res
                      << "\nInt res: " << val1 + val2 << std::endl;
            // LCOV_EXCL_STOP
        }
    }
}

template <typename T>
void test_comparisions()
{
    std::uniform_int_distribution<std::int64_t> dist(-1000, 1000);

    for (std::size_t i {}; i < N; ++i)
    {
        const T val1 {dist(rng)};
        const T val2 {dist(rng)};

        const T dec1 {val1};
        const T dec2 {val2};

        BOOST_TEST_EQ(val1 == val2, dec1 == dec2);
        BOOST_TEST_EQ(val1 != val2, dec1 != dec2);
        BOOST_TEST_EQ(val1 < val2, dec1 < dec2);
        BOOST_TEST_EQ(val1 <= val2, dec1 <= dec2);
        BOOST_TEST_EQ(val1 > val2, dec1 > dec2);
        BOOST_TEST_EQ(val1 >= val2, dec1 >= dec2);
    }
}

int main()
{
    random_addition<decimal32_t>();
    random_subtraction<decimal32_t>();
    random_multiplication<decimal32_t>();
    random_division<decimal32_t>();
    test_comparisions<decimal32_t>();

    random_addition<decimal_fast32_t>();
    random_subtraction<decimal_fast32_t>();
    random_multiplication<decimal_fast32_t>();
    random_division<decimal_fast32_t>();
    test_comparisions<decimal_fast32_t>();

    random_addition<decimal64_t>();
    random_subtraction<decimal64_t>();
    random_multiplication<decimal64_t>();
    random_division<decimal64_t>();
    test_comparisions<decimal64_t>();

    random_addition<decimal_fast64_t>();
    random_subtraction<decimal_fast64_t>();
    random_multiplication<decimal_fast64_t>();
    random_division<decimal_fast64_t>();
    test_comparisions<decimal_fast64_t>();

    #if !defined(BOOST_DECIMAL_REDUCE_TEST_DEPTH)
    random_addition<decimal128_t>();
    random_subtraction<decimal128_t>();
    random_multiplication<decimal128_t>();
    random_division<decimal128_t>();
    test_comparisions<decimal128_t>();
    #endif

    return boost::report_errors();
}
