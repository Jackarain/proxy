// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

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

template <typename Decimal1, typename Decimal2>
void random_mixed_EQ()
{
    std::uniform_int_distribution<int> dist(-9'999'999, 9'999'999);

    constexpr auto max_iter {std::is_same<Decimal2, decimal128_t>::value ? N / 4 : N};
    for (std::size_t i {}; i < max_iter; ++i)
    {
        const int val1 {dist(rng)};
        const int val2 {dist(rng)};

        const Decimal1 dec1 {val1};
        const Decimal2 dec2 {val2};

        if (!BOOST_TEST_EQ(dec1 == dec2, val1 == val2))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << dec1
                      << "\nVal 2: " << val2
                      << "\nDec 2: " << dec2 << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    for (std::size_t i {}; i < max_iter; ++i)
    {
        const int val1 {dist(rng)};
        const int val2 {dist(rng)};

        const Decimal2 dec1 {val1};
        const Decimal1 dec2 {val2};

        if (!BOOST_TEST_EQ(dec1 == dec2, val1 == val2))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << dec1
                      << "\nVal 2: " << val2
                      << "\nDec 2: " << dec2 << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    // Edge cases
    const auto guarantee {dist(rng)};
    if (!BOOST_TEST(Decimal2{guarantee} == Decimal1{guarantee}))
    {
        // LCOV_EXCL_START
        std::cerr << "Dec64: " << Decimal2{guarantee}
                  << "\nDec32: " << Decimal1{guarantee} << std::endl;
        // LCOV_EXCL_STOP
    }

    BOOST_TEST_EQ(std::numeric_limits<Decimal1>::quiet_NaN() == Decimal2(dist(rng)), false);
    BOOST_TEST_EQ(std::numeric_limits<Decimal2>::quiet_NaN() == Decimal1(dist(rng)), false);
    BOOST_TEST_EQ(std::numeric_limits<Decimal1>::infinity() == Decimal2(dist(rng)), false);
    BOOST_TEST_EQ(std::numeric_limits<Decimal2>::infinity() == Decimal1(dist(rng)), false);
}

template <typename Decimal1, typename Decimal2>
void random_mixed_NE()
{
    std::uniform_int_distribution<int> dist(-9'999'999, 9'999'999);

    constexpr auto max_iter {std::is_same<Decimal2, decimal128_t>::value ? N / 4 : N};
    for (std::size_t i {}; i < max_iter; ++i)
    {
        const int val1 {dist(rng)};
        const int val2 {dist(rng)};

        const Decimal1 dec1 {val1};
        const Decimal2 dec2 {val2};

        if (!BOOST_TEST_EQ(dec1 != dec2, val1 != val2))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << dec1
                      << "\nVal 2: " << val2
                      << "\nDec 2: " << dec2 << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    for (std::size_t i {}; i < max_iter; ++i)
    {
        const int val1 {dist(rng)};
        const int val2 {dist(rng)};

        const Decimal2 dec1 {val1};
        const Decimal1 dec2 {val2};

        if (!BOOST_TEST_EQ(dec1 != dec2, val1 != val2))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << dec1
                      << "\nVal 2: " << val2
                      << "\nDec 2: " << dec2 << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    // Edge cases
    BOOST_TEST(std::numeric_limits<Decimal1>::quiet_NaN() != Decimal2(dist(rng)));
    BOOST_TEST(std::numeric_limits<Decimal2>::quiet_NaN() != Decimal1(dist(rng)));
    BOOST_TEST(std::numeric_limits<Decimal1>::infinity() != Decimal2(dist(rng)));
    BOOST_TEST(std::numeric_limits<Decimal2>::infinity() != Decimal1(dist(rng)));
}

template <typename Decimal1, typename Decimal2>
void random_mixed_LT()
{
    std::uniform_int_distribution<int> dist(-9'999'999, 9'999'999);

    constexpr auto max_iter {std::is_same<Decimal2, decimal128_t>::value ? N / 4 : N};
    for (std::size_t i {}; i < max_iter; ++i)
    {
        const int val1 {dist(rng)};
        const int val2 {dist(rng)};

        const Decimal1 dec1 {val1};
        const Decimal2 dec2 {val2};

        if (!BOOST_TEST_EQ(dec1 < dec2, val1 < val2))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << dec1
                      << "\nVal 2: " << val2
                      << "\nDec 2: " << dec2 << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    for (std::size_t i {}; i < max_iter; ++i)
    {
        const int val1 {dist(rng)};
        const int val2 {dist(rng)};

        const Decimal2 dec1 {val1};
        const Decimal1 dec2 {val2};

        if (!BOOST_TEST_EQ(dec1 < dec2, val1 < val2))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << dec1
                      << "\nVal 2: " << val2
                      << "\nDec 2: " << dec2 << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    // Edge cases
    BOOST_TEST_EQ(std::numeric_limits<Decimal1>::quiet_NaN() < Decimal2(dist(rng)), false);
    BOOST_TEST_EQ(std::numeric_limits<Decimal2>::quiet_NaN() < Decimal1(dist(rng)), false);
    BOOST_TEST_EQ(std::numeric_limits<Decimal1>::infinity() < Decimal2(dist(rng)), false);
    BOOST_TEST_EQ(std::numeric_limits<Decimal2>::infinity() < Decimal1(dist(rng)), false);
    BOOST_TEST_EQ(Decimal1(dist(rng)) < std::numeric_limits<Decimal2>::infinity(), true);
    BOOST_TEST_EQ(Decimal2(dist(rng)) < std::numeric_limits<Decimal1>::infinity(), true);
    BOOST_TEST_EQ(Decimal1(dist(rng)) < -std::numeric_limits<Decimal2>::infinity(), false);
    BOOST_TEST_EQ(Decimal2(dist(rng)) < -std::numeric_limits<Decimal1>::infinity(), false);
}

template <typename Decimal1, typename Decimal2>
void random_mixed_LE()
{
    std::uniform_int_distribution<int> dist(-9'999'999, 9'999'999);

    constexpr auto max_iter {std::is_same<Decimal2, decimal128_t>::value ? N / 4 : N};
    for (std::size_t i {}; i < max_iter; ++i)
    {
        const int val1 {dist(rng)};
        const int val2 {dist(rng)};

        const Decimal1 dec1 {val1};
        const Decimal2 dec2 {val2};

        if (!BOOST_TEST_EQ(dec1 <= dec2, val1 <= val2))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << dec1
                      << "\nVal 2: " << val2
                      << "\nDec 2: " << dec2 << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    for (std::size_t i {}; i < max_iter; ++i)
    {
        const int val1 {dist(rng)};
        const int val2 {dist(rng)};

        const Decimal2 dec1 {val1};
        const Decimal1 dec2 {val2};

        if (!BOOST_TEST_EQ(dec1 <= dec2, val1 <= val2))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << dec1
                      << "\nVal 2: " << val2
                      << "\nDec 2: " << dec2 << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    // Edge cases
    BOOST_TEST_EQ(std::numeric_limits<Decimal1>::quiet_NaN() <= Decimal2(dist(rng)), false);
    BOOST_TEST_EQ(std::numeric_limits<Decimal2>::quiet_NaN() <= Decimal1(dist(rng)), false);
    BOOST_TEST_EQ(std::numeric_limits<Decimal1>::infinity() <= Decimal2(dist(rng)), false);
    BOOST_TEST_EQ(std::numeric_limits<Decimal2>::infinity() <= Decimal1(dist(rng)), false);
}

template <typename Decimal1, typename Decimal2>
void random_mixed_GT()
{
    std::uniform_int_distribution<int> dist(-9'999'999, 9'999'999);

    constexpr auto max_iter {std::is_same<Decimal2, decimal128_t>::value ? N / 4 : N};
    for (std::size_t i {}; i < max_iter; ++i)
    {
        const int val1 {dist(rng)};
        const int val2 {dist(rng)};

        const Decimal1 dec1 {val1};
        const Decimal2 dec2 {val2};

        if (!BOOST_TEST_EQ(dec1 > dec2, val1 > val2))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << dec1
                      << "\nVal 2: " << val2
                      << "\nDec 2: " << dec2 << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    for (std::size_t i {}; i < max_iter; ++i)
    {
        const int val1 {dist(rng)};
        const int val2 {dist(rng)};

        const Decimal2 dec1 {val1};
        const Decimal1 dec2 {val2};

        if (!BOOST_TEST_EQ(dec1 > dec2, val1 > val2))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << dec1
                      << "\nVal 2: " << val2
                      << "\nDec 2: " << dec2 << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    // Edge cases
    BOOST_TEST_EQ(std::numeric_limits<Decimal1>::quiet_NaN() > Decimal2(dist(rng)), false);
    BOOST_TEST_EQ(std::numeric_limits<Decimal2>::quiet_NaN() > Decimal1(dist(rng)), false);
    BOOST_TEST_EQ(std::numeric_limits<Decimal1>::infinity() > Decimal2(dist(rng)), true);
    BOOST_TEST_EQ(std::numeric_limits<Decimal2>::infinity() > Decimal1(dist(rng)), true);
}

template <typename Decimal1, typename Decimal2>
void random_mixed_GE()
{
    std::uniform_int_distribution<int> dist(-9'999'999, 9'999'999);

    constexpr auto max_iter {std::is_same<Decimal2, decimal128_t>::value ? N / 4 : N};
    for (std::size_t i {}; i < max_iter; ++i)
    {
        const int val1 {dist(rng)};
        const int val2 {dist(rng)};

        const Decimal1 dec1 {val1};
        const Decimal2 dec2 {val2};

        if (!BOOST_TEST_EQ(dec1 >= dec2, val1 >= val2))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << dec1
                      << "\nVal 2: " << val2
                      << "\nDec 2: " << dec2 << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    for (std::size_t i {}; i < max_iter; ++i)
    {
        const int val1 {dist(rng)};
        const int val2 {dist(rng)};

        const Decimal2 dec1 {val1};
        const Decimal1 dec2 {val2};

        if (!BOOST_TEST_EQ(dec1 >= dec2, val1 >= val2))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << dec1
                      << "\nVal 2: " << val2
                      << "\nDec 2: " << dec2 << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    // Edge cases
    const auto guarantee {dist(rng)};
    if (!BOOST_TEST(Decimal2{guarantee} >= Decimal1{guarantee}))
    {
        // LCOV_EXCL_START
        std::cerr << "Dec64: " << Decimal2{guarantee}
                  << "\nDec32: " << Decimal1{guarantee} << std::endl;
        // LCOV_EXCL_STOP
    }

    BOOST_TEST_EQ(std::numeric_limits<Decimal1>::quiet_NaN() >= Decimal2(dist(rng)), false);
    BOOST_TEST_EQ(std::numeric_limits<Decimal2>::quiet_NaN() >= Decimal1(dist(rng)), false);
    BOOST_TEST_EQ(std::numeric_limits<Decimal1>::infinity() >= Decimal2(dist(rng)), true);
    BOOST_TEST_EQ(std::numeric_limits<Decimal2>::infinity() >= Decimal1(dist(rng)), true);
}

#ifdef BOOST_DECIMAL_HAS_SPACESHIP_OPERATOR

template <typename Decimal1, typename Decimal2>
void random_mixed_SPACESHIP()
{
    std::uniform_int_distribution<int> dist(-9'999'999, 9'999'999);

    constexpr auto max_iter {std::is_same<Decimal2, decimal128_t>::value ? N / 4 : N};
    for (std::size_t i {}; i < max_iter; ++i)
    {
        const int val1 {dist(rng)};
        const int val2 {dist(rng)};

        const Decimal1 dec1 {val1};
        const Decimal2 dec2 {val2};

        if (!BOOST_TEST((dec1 <=> dec2) == (val1 <=> val2)))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << dec1
                      << "\nVal 2: " << val2
                      << "\nDec 2: " << dec2 << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    for (std::size_t i {}; i < max_iter; ++i)
    {
        const int val1 {dist(rng)};
        const int val2 {dist(rng)};

        const Decimal2 dec1 {val1};
        const Decimal1 dec2 {val2};

        if (!BOOST_TEST((dec1 <=> dec2) == (val1 <=> val2)))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << dec1
                      << "\nVal 2: " << val2
                      << "\nDec 2: " << dec2 << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    // Edge cases
    const auto guarantee {dist(rng)};
    if (!BOOST_TEST((Decimal2{guarantee} <=> Decimal1{guarantee}) == std::partial_ordering::equivalent))
    {
        // LCOV_EXCL_START
        std::cerr << "Dec64: " << Decimal2{guarantee}
                  << "\nDec32: " << Decimal1{guarantee} << std::endl;
        // LCOV_EXCL_STOP
    }

    BOOST_TEST((std::numeric_limits<Decimal1>::quiet_NaN() <=> Decimal2(dist(rng))) == std::partial_ordering::unordered);
    BOOST_TEST((std::numeric_limits<Decimal2>::quiet_NaN() <=> Decimal1(dist(rng))) == std::partial_ordering::unordered);
    BOOST_TEST((std::numeric_limits<Decimal1>::infinity() <=> Decimal2(dist(rng))) == std::partial_ordering::greater);
    BOOST_TEST((std::numeric_limits<Decimal2>::infinity() <=> Decimal1(dist(rng))) == std::partial_ordering::greater);
}

#endif

template <typename Decimal1, typename Decimal2>
void random_conversion_EQ()
{
    std::uniform_int_distribution<int> dist(-9'999'999, 9'999'999);

    constexpr auto max_iter {std::is_same<Decimal2, decimal128_t>::value ? N / 4 : N};
    for (std::size_t i {}; i < max_iter; ++i)
    {
        const int val1 {dist(rng)};
        const int val2 {dist(rng)};

        const Decimal1 dec1 {val1};
        const Decimal2 dec2 {Decimal1(val2)};

        if (!BOOST_TEST_EQ(dec1 == dec2, val1 == val2))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << dec1
                      << "\nVal 2: " << val2
                      << "\nDec 2: " << dec2 << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    for (std::size_t i {}; i < max_iter; ++i)
    {
        const int val1 {dist(rng)};
        const int val2 {dist(rng)};

        const Decimal2 dec1 {val1};
        const Decimal1 dec2 {Decimal2(val2)};

        if (!BOOST_TEST_EQ(dec1 == dec2, val1 == val2))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << dec1
                      << "\nVal 2: " << val2
                      << "\nDec 2: " << dec2 << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    // Edge cases
    const auto guarantee {dist(rng)};
    if (!BOOST_TEST(Decimal1(Decimal2{guarantee}) == Decimal1{guarantee}))
    {
        // LCOV_EXCL_START
        std::cerr << "Dec from Dec: " << Decimal1(Decimal2{guarantee})
                  << "\n       Dec32: " << Decimal1{guarantee} << std::endl;
        // LCOV_EXCL_STOP
    }

    BOOST_TEST_EQ(std::numeric_limits<Decimal1>::quiet_NaN() == Decimal2(dist(rng)), false);
    BOOST_TEST_EQ(std::numeric_limits<Decimal2>::quiet_NaN() == Decimal1(dist(rng)), false);
    BOOST_TEST_EQ(std::numeric_limits<Decimal1>::infinity() == Decimal2(dist(rng)), false);
    BOOST_TEST_EQ(std::numeric_limits<Decimal2>::infinity() == Decimal1(dist(rng)), false);
}

int main()
{
    random_mixed_EQ<decimal32_t, decimal64_t>();
    random_mixed_NE<decimal32_t, decimal64_t>();
    random_mixed_LT<decimal32_t, decimal64_t>();
    random_mixed_LE<decimal32_t, decimal64_t>();
    random_mixed_GT<decimal32_t, decimal64_t>();
    random_mixed_GE<decimal32_t, decimal64_t>();

    random_conversion_EQ<decimal32_t, decimal64_t>();

    random_mixed_EQ<decimal32_t, decimal128_t>();
    random_mixed_NE<decimal32_t, decimal128_t>();
    random_mixed_LT<decimal32_t, decimal128_t>();
    random_mixed_LE<decimal32_t, decimal128_t>();
    random_mixed_GT<decimal32_t, decimal128_t>();
    random_mixed_GE<decimal32_t, decimal128_t>();

    random_conversion_EQ<decimal32_t, decimal128_t>();

    random_mixed_EQ<decimal64_t, decimal128_t>();
    random_mixed_NE<decimal64_t, decimal128_t>();
    random_mixed_LT<decimal64_t, decimal128_t>();
    random_mixed_LE<decimal64_t, decimal128_t>();
    random_mixed_GT<decimal64_t, decimal128_t>();
    random_mixed_GE<decimal64_t, decimal128_t>();

    random_conversion_EQ<decimal64_t, decimal128_t>();


    #ifdef BOOST_DECIMAL_HAS_SPACESHIP_OPERATOR
    random_mixed_SPACESHIP<decimal32_t, decimal64_t>();
    random_mixed_SPACESHIP<decimal32_t, decimal128_t>();
    random_mixed_SPACESHIP<decimal64_t, decimal128_t>();
    #endif

    return boost::report_errors();
}
