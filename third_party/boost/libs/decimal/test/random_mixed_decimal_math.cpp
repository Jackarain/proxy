// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include "testing_config.hpp"
#include <boost/decimal.hpp>
#include <random>
#include <limits>

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
static constexpr auto N = static_cast<std::size_t>(1024); // Number of trials
#else
static constexpr auto N = static_cast<std::size_t>(1024 >> 4U); // Number of trials
#endif

// NOLINTNEXTLINE : Seed with a constant for repeatability
static std::mt19937_64 rng(42); // NOSONAR : Global rng is not const

#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable: 4146)
#endif

template <typename Decimal1, typename Decimal2, typename T>
void random_mixed_addition(T lower, T upper)
{
    std::uniform_int_distribution<T> dist(lower, upper);

    constexpr auto max_iter {(std::is_same<Decimal2, decimal128_t>::value || std::is_same<Decimal1, decimal128_t>::value) ? N / 4 : N};
    for (std::size_t i {}; i < max_iter; ++i)
    {
        const T val1 {dist(rng)};
        const T val2 {dist(rng)};

        const Decimal1 dec1 {val1};
        const Decimal2 dec2 {val2};

        const auto res = dec1 + dec2;
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

    BOOST_TEST(isinf(std::numeric_limits<Decimal1>::infinity() + Decimal2(dist(rng))));
    BOOST_TEST(isinf(Decimal2(dist(rng)) + std::numeric_limits<Decimal1>::infinity()));
    BOOST_TEST(isnan(std::numeric_limits<Decimal1>::quiet_NaN() + Decimal2(dist(rng))));
    BOOST_TEST(isnan(Decimal2(dist(rng)) + std::numeric_limits<Decimal1>::quiet_NaN()));
    BOOST_TEST(isnan(std::numeric_limits<Decimal1>::signaling_NaN() + Decimal2(dist(rng))));
    BOOST_TEST(isnan(Decimal2(dist(rng)) + std::numeric_limits<Decimal1>::signaling_NaN()));
}

template <typename Decimal1, typename Decimal2, typename T>
void random_mixed_subtraction(T lower, T upper)
{
    std::uniform_int_distribution<T> dist(lower, upper);

    constexpr auto max_iter {(std::is_same<Decimal2, decimal128_t>::value || std::is_same<Decimal1, decimal128_t>::value) ? N / 4 : N};
    for (std::size_t i {}; i < max_iter; ++i)
    {
        const T val1 {dist(rng)};
        const T val2 {dist(rng)};

        const Decimal1 dec1 {val1};
        const Decimal2 dec2 {val2};

        const auto res = dec1 - dec2;
        const auto res_int = static_cast<T>(res);

        if (!BOOST_TEST_EQ(res_int, val1 - val2))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << dec1
                      << "\nVal 2: " << val2
                      << "\nDec 2: " << dec2
                      << "\nDec res: " << res
                      << "\nInt res: " << val1 - val2 << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    BOOST_TEST(isinf(std::numeric_limits<Decimal1>::infinity() - Decimal2(dist(rng))));
    BOOST_TEST(isinf(Decimal2(dist(rng)) - std::numeric_limits<Decimal1>::infinity()));
    BOOST_TEST(isnan(std::numeric_limits<Decimal1>::quiet_NaN() - Decimal2(dist(rng))));
    BOOST_TEST(isnan(Decimal2(dist(rng)) - std::numeric_limits<Decimal1>::quiet_NaN()));
    BOOST_TEST(isnan(std::numeric_limits<Decimal1>::signaling_NaN() - Decimal2(dist(rng))));
    BOOST_TEST(isnan(Decimal2(dist(rng)) - std::numeric_limits<Decimal1>::signaling_NaN()));
}

template <typename Decimal1, typename Decimal2, typename T>
void random_mixed_multiplication(T lower, T upper)
{
    std::uniform_int_distribution<T> dist(lower, upper);

    constexpr auto max_iter {(std::is_same<Decimal2, decimal128_t>::value || std::is_same<Decimal1, decimal128_t>::value) ? N / 4 : N};
    for (std::size_t i {}; i < max_iter; ++i)
    {
        const T val1 {dist(rng)};
        const T val2 {dist(rng)};

        const Decimal1 dec1 {val1};
        const Decimal2 dec2 {val2};

        const auto res = dec1 * dec2;
        const auto res_int = static_cast<T>(res);

        if (!BOOST_TEST_EQ(res_int, val1 * val2))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << dec1
                      << "\nVal 2: " << val2
                      << "\nDec 2: " << dec2
                      << "\nDec res: " << res
                      << "\nInt res: " << val1 * val2 << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    BOOST_TEST(isinf(std::numeric_limits<Decimal1>::infinity() * Decimal2(dist(rng))));
    BOOST_TEST(isinf(Decimal2(dist(rng)) * std::numeric_limits<Decimal1>::infinity()));
    BOOST_TEST(isnan(std::numeric_limits<Decimal1>::quiet_NaN() * Decimal2(dist(rng))));
    BOOST_TEST(isnan(Decimal2(dist(rng)) * std::numeric_limits<Decimal1>::quiet_NaN()));
    BOOST_TEST(isnan(std::numeric_limits<Decimal1>::signaling_NaN() * Decimal2(dist(rng))));
    BOOST_TEST(isnan(Decimal2(dist(rng)) * std::numeric_limits<Decimal1>::signaling_NaN()));
}

template <typename Decimal1, typename Decimal2, typename T>
void random_mixed_division(T lower, T upper)
{
    std::uniform_int_distribution<T> dist(lower, upper);

    constexpr auto max_iter {(std::is_same<Decimal2, decimal128_t>::value || std::is_same<Decimal1, decimal128_t>::value) ? N / 4 : N};
    for (std::size_t i {}; i < max_iter; ++i)
    {
        const T val1 {dist(rng)};
        const T val2 {dist(rng)};

        const Decimal1 dec1 {val1};
        const Decimal2 dec2 {val2};

        const auto res {dec1 / dec2};
        const decltype(res) res_int {static_cast<double>(val1) / static_cast<double>(val2)};

        if (isinf(res) && isinf(res_int))
        {
        }
        else if (!BOOST_TEST_EQ(static_cast<float>(res), static_cast<float>(res_int)))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << dec1
                      << "\nVal 2: " << val2
                      << "\nDec 2: " << dec2
                      << "\nDec res: " << res
                      << "\nInt res: " << static_cast<double>(val1) / static_cast<double>(val2) << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    BOOST_TEST(!isinf(Decimal2(dist(rng)) / std::numeric_limits<Decimal1>::infinity()));
    BOOST_TEST(isnan(std::numeric_limits<Decimal1>::quiet_NaN() / Decimal2(dist(rng))));
    BOOST_TEST(isnan(Decimal2(dist(rng)) / std::numeric_limits<Decimal1>::quiet_NaN()));
    BOOST_TEST(isnan(std::numeric_limits<Decimal1>::signaling_NaN() / Decimal2(dist(rng))));
    BOOST_TEST(isnan(Decimal2(dist(rng)) / std::numeric_limits<Decimal1>::signaling_NaN()));
    BOOST_TEST(isinf(Decimal2(dist(rng)) / Decimal1(0)));
}

int main()
{
    random_mixed_addition<decimal32_t, decimal64_t>(0, 5'000'000);
    random_mixed_addition<decimal32_t, decimal64_t>(0LL, 5'000'000LL);
    random_mixed_addition<decimal64_t, decimal32_t>(0, 5'000'000);
    random_mixed_addition<decimal64_t, decimal32_t>(0LL, 5'000'000LL);

    random_mixed_addition<decimal32_t, decimal64_t>(-5'000'000, 0);
    random_mixed_addition<decimal32_t, decimal64_t>(-5'000'000LL, 0LL);
    random_mixed_addition<decimal64_t, decimal32_t>(-5'000'000, 0);
    random_mixed_addition<decimal64_t, decimal32_t>(-5'000'000LL, 0LL);

    random_mixed_addition<decimal32_t, decimal64_t>(-5'000'000, 5'000'000);
    random_mixed_addition<decimal32_t, decimal64_t>(-5'000'000LL, 5'000'000LL);
    random_mixed_addition<decimal64_t, decimal32_t>(-5'000'000, 5'000'000);
    random_mixed_addition<decimal64_t, decimal32_t>(-5'000'000LL, 5'000'000LL);

    random_mixed_subtraction<decimal32_t, decimal64_t>(0, 5'000'000);
    random_mixed_subtraction<decimal32_t, decimal64_t>(0LL, 5'000'000LL);
    random_mixed_subtraction<decimal64_t, decimal32_t>(0, 5'000'000);
    random_mixed_subtraction<decimal64_t, decimal32_t>(0LL, 5'000'000LL);

    random_mixed_subtraction<decimal32_t, decimal64_t>(-5'000'000, 0);
    random_mixed_subtraction<decimal32_t, decimal64_t>(-5'000'000LL, 0LL);
    random_mixed_subtraction<decimal64_t, decimal32_t>(-5'000'000, 0);
    random_mixed_subtraction<decimal64_t, decimal32_t>(-5'000'000LL, 0LL);

    random_mixed_subtraction<decimal32_t, decimal64_t>(-5'000'000, 5'000'000);
    random_mixed_subtraction<decimal32_t, decimal64_t>(-5'000'000LL, 5'000'000LL);
    random_mixed_subtraction<decimal64_t, decimal32_t>(-5'000'000, 5'000'000);
    random_mixed_subtraction<decimal64_t, decimal32_t>(-5'000'000LL, 5'000'000LL);

    random_mixed_multiplication<decimal32_t, decimal64_t>(0, 5'000);
    random_mixed_multiplication<decimal32_t, decimal64_t>(0LL, 5'000LL);
    random_mixed_multiplication<decimal64_t, decimal32_t>(0, 5'000);
    random_mixed_multiplication<decimal64_t, decimal32_t>(0LL, 5'000LL);

    random_mixed_multiplication<decimal32_t, decimal64_t>(-5'000, 0);
    random_mixed_multiplication<decimal32_t, decimal64_t>(-5'000LL, 0LL);
    random_mixed_multiplication<decimal64_t, decimal32_t>(-5'000, 0);
    random_mixed_multiplication<decimal64_t, decimal32_t>(-5'000LL, 0LL);

    random_mixed_multiplication<decimal32_t, decimal64_t>(-5'000, 5'000);
    random_mixed_multiplication<decimal32_t, decimal64_t>(-5'000LL, 5'000LL);
    random_mixed_multiplication<decimal64_t, decimal32_t>(-5'000, 5'000);
    random_mixed_multiplication<decimal64_t, decimal32_t>(-5'000LL, 5'000LL);

    random_mixed_division<decimal32_t, decimal64_t>(0, 5'000);
    random_mixed_division<decimal32_t, decimal64_t>(0LL, 5'000LL);
    random_mixed_division<decimal64_t, decimal32_t>(0, 5'000);
    random_mixed_division<decimal64_t, decimal32_t>(0LL, 5'000LL);

    random_mixed_division<decimal32_t, decimal64_t>(-5'000, 0);
    random_mixed_division<decimal32_t, decimal64_t>(-5'000LL, 0LL);
    random_mixed_division<decimal64_t, decimal32_t>(-5'000, 0);
    random_mixed_division<decimal64_t, decimal32_t>(-5'000LL, 0LL);

    random_mixed_division<decimal32_t, decimal64_t>(-5'000, 5'000);
    random_mixed_division<decimal32_t, decimal64_t>(-5'000LL, 5'000LL);
    random_mixed_division<decimal64_t, decimal32_t>(-5'000, 5'000);
    random_mixed_division<decimal64_t, decimal32_t>(-5'000LL, 5'000LL);

    random_mixed_addition<decimal32_t, decimal128_t>(0, 5'000'000);
    random_mixed_addition<decimal32_t, decimal128_t>(0LL, 5'000'000LL);
    random_mixed_addition<decimal128_t, decimal32_t>(0, 5'000'000);
    random_mixed_addition<decimal128_t, decimal32_t>(0LL, 5'000'000LL);

    random_mixed_addition<decimal32_t, decimal128_t>(-5'000'000, 0);
    random_mixed_addition<decimal32_t, decimal128_t>(-5'000'000LL, 0LL);
    random_mixed_addition<decimal128_t, decimal32_t>(-5'000'000, 0);
    random_mixed_addition<decimal128_t, decimal32_t>(-5'000'000LL, 0LL);

    random_mixed_addition<decimal32_t, decimal128_t>(-5'000'000, 5'000'000);
    random_mixed_addition<decimal32_t, decimal128_t>(-5'000'000LL, 5'000'000LL);
    random_mixed_addition<decimal128_t, decimal32_t>(-5'000'000, 5'000'000);
    random_mixed_addition<decimal128_t, decimal32_t>(-5'000'000LL, 5'000'000LL);

    random_mixed_subtraction<decimal32_t, decimal128_t>(0, 5'000'000);
    random_mixed_subtraction<decimal32_t, decimal128_t>(0LL, 5'000'000LL);
    random_mixed_subtraction<decimal128_t, decimal32_t>(0, 5'000'000);
    random_mixed_subtraction<decimal128_t, decimal32_t>(0LL, 5'000'000LL);

    random_mixed_subtraction<decimal32_t, decimal128_t>(-5'000'000, 0);
    random_mixed_subtraction<decimal32_t, decimal128_t>(-5'000'000LL, 0LL);
    random_mixed_subtraction<decimal128_t, decimal32_t>(-5'000'000, 0);
    random_mixed_subtraction<decimal128_t, decimal32_t>(-5'000'000LL, 0LL);

    random_mixed_subtraction<decimal32_t, decimal128_t>(-5'000'000, 5'000'000);
    random_mixed_subtraction<decimal32_t, decimal128_t>(-5'000'000LL, 5'000'000LL);
    random_mixed_subtraction<decimal128_t, decimal32_t>(-5'000'000, 5'000'000);
    random_mixed_subtraction<decimal128_t, decimal32_t>(-5'000'000LL, 5'000'000LL);

    random_mixed_multiplication<decimal32_t, decimal128_t>(0, 5'000);
    random_mixed_multiplication<decimal32_t, decimal128_t>(0LL, 5'000LL);
    random_mixed_multiplication<decimal128_t, decimal32_t>(0, 5'000);
    random_mixed_multiplication<decimal128_t, decimal32_t>(0LL, 5'000LL);

    random_mixed_multiplication<decimal32_t, decimal128_t>(-5'000, 0);
    random_mixed_multiplication<decimal32_t, decimal128_t>(-5'000LL, 0LL);
    random_mixed_multiplication<decimal128_t, decimal32_t>(-5'000, 0);
    random_mixed_multiplication<decimal128_t, decimal32_t>(-5'000LL, 0LL);

    random_mixed_multiplication<decimal32_t, decimal128_t>(-5'000, 5'000);
    random_mixed_multiplication<decimal32_t, decimal128_t>(-5'000LL, 5'000LL);
    random_mixed_multiplication<decimal128_t, decimal32_t>(-5'000, 5'000);
    random_mixed_multiplication<decimal128_t, decimal32_t>(-5'000LL, 5'000LL);

    random_mixed_division<decimal32_t, decimal128_t>(0, 5'000);
    random_mixed_division<decimal32_t, decimal128_t>(0LL, 5'000LL);
    random_mixed_division<decimal128_t, decimal32_t>(0, 5'000);
    random_mixed_division<decimal128_t, decimal32_t>(0LL, 5'000LL);

    random_mixed_division<decimal32_t, decimal128_t>(-5'000, 0);
    random_mixed_division<decimal32_t, decimal128_t>(-5'000LL, 0LL);
    random_mixed_division<decimal128_t, decimal32_t>(-5'000, 0);
    random_mixed_division<decimal128_t, decimal32_t>(-5'000LL, 0LL);

    random_mixed_division<decimal32_t, decimal128_t>(-5'000, 5'000);
    random_mixed_division<decimal32_t, decimal128_t>(-5'000LL, 5'000LL);
    random_mixed_division<decimal128_t, decimal32_t>(-5'000, 5'000);
    random_mixed_division<decimal128_t, decimal32_t>(-5'000LL, 5'000LL);
    
    random_mixed_addition<decimal64_t, decimal128_t>(0, 5'000'000);
    random_mixed_addition<decimal64_t, decimal128_t>(0LL, 5'000'000LL);
    random_mixed_addition<decimal128_t, decimal64_t>(0, 5'000'000);
    random_mixed_addition<decimal128_t, decimal64_t>(0LL, 5'000'000LL);

    random_mixed_addition<decimal64_t, decimal128_t>(-5'000'000, 0);
    random_mixed_addition<decimal64_t, decimal128_t>(-5'000'000LL, 0LL);
    random_mixed_addition<decimal128_t, decimal64_t>(-5'000'000, 0);
    random_mixed_addition<decimal128_t, decimal64_t>(-5'000'000LL, 0LL);

    random_mixed_addition<decimal64_t, decimal128_t>(-5'000'000, 5'000'000);
    random_mixed_addition<decimal64_t, decimal128_t>(-5'000'000LL, 5'000'000LL);
    random_mixed_addition<decimal128_t, decimal64_t>(-5'000'000, 5'000'000);
    random_mixed_addition<decimal128_t, decimal64_t>(-5'000'000LL, 5'000'000LL);

    random_mixed_subtraction<decimal64_t, decimal128_t>(0, 5'000'000);
    random_mixed_subtraction<decimal64_t, decimal128_t>(0LL, 5'000'000LL);
    random_mixed_subtraction<decimal128_t, decimal64_t>(0, 5'000'000);
    random_mixed_subtraction<decimal128_t, decimal64_t>(0LL, 5'000'000LL);

    random_mixed_subtraction<decimal64_t, decimal128_t>(-5'000'000, 0);
    random_mixed_subtraction<decimal64_t, decimal128_t>(-5'000'000LL, 0LL);
    random_mixed_subtraction<decimal128_t, decimal64_t>(-5'000'000, 0);
    random_mixed_subtraction<decimal128_t, decimal64_t>(-5'000'000LL, 0LL);

    random_mixed_subtraction<decimal64_t, decimal128_t>(-5'000'000, 5'000'000);
    random_mixed_subtraction<decimal64_t, decimal128_t>(-5'000'000LL, 5'000'000LL);
    random_mixed_subtraction<decimal128_t, decimal64_t>(-5'000'000, 5'000'000);
    random_mixed_subtraction<decimal128_t, decimal64_t>(-5'000'000LL, 5'000'000LL);

    random_mixed_multiplication<decimal64_t, decimal128_t>(0, 5'000);
    random_mixed_multiplication<decimal64_t, decimal128_t>(0LL, 5'000LL);
    random_mixed_multiplication<decimal128_t, decimal64_t>(0, 5'000);
    random_mixed_multiplication<decimal128_t, decimal64_t>(0LL, 5'000LL);

    random_mixed_multiplication<decimal64_t, decimal128_t>(-5'000, 0);
    random_mixed_multiplication<decimal64_t, decimal128_t>(-5'000LL, 0LL);
    random_mixed_multiplication<decimal128_t, decimal64_t>(-5'000, 0);
    random_mixed_multiplication<decimal128_t, decimal64_t>(-5'000LL, 0LL);

    random_mixed_multiplication<decimal64_t, decimal128_t>(-5'000, 5'000);
    random_mixed_multiplication<decimal64_t, decimal128_t>(-5'000LL, 5'000LL);
    random_mixed_multiplication<decimal128_t, decimal64_t>(-5'000, 5'000);
    random_mixed_multiplication<decimal128_t, decimal64_t>(-5'000LL, 5'000LL);

    random_mixed_division<decimal64_t, decimal128_t>(0, 5'000);
    random_mixed_division<decimal64_t, decimal128_t>(0LL, 5'000LL);
    random_mixed_division<decimal128_t, decimal64_t>(0, 5'000);
    random_mixed_division<decimal128_t, decimal64_t>(0LL, 5'000LL);

    random_mixed_division<decimal64_t, decimal128_t>(-5'000, 0);
    random_mixed_division<decimal64_t, decimal128_t>(-5'000LL, 0LL);
    random_mixed_division<decimal128_t, decimal64_t>(-5'000, 0);
    random_mixed_division<decimal128_t, decimal64_t>(-5'000LL, 0LL);

    random_mixed_division<decimal64_t, decimal128_t>(-5'000, 5'000);
    random_mixed_division<decimal64_t, decimal128_t>(-5'000LL, 5'000LL);
    random_mixed_division<decimal128_t, decimal64_t>(-5'000, 5'000);
    random_mixed_division<decimal128_t, decimal64_t>(-5'000LL, 5'000LL);

    return boost::report_errors();
}

#ifdef _MSC_VER
#  pragma warning(pop)
#endif
