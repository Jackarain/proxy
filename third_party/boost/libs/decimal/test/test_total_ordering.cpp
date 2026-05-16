// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/decimal.hpp>
#include <boost/core/lightweight_test.hpp>
#include <random>
#include <limits>
#include <string>
#include <climits>

using namespace boost::decimal;

static std::mt19937_64 rng(42);
static std::uniform_int_distribution<int> dist(INT_MIN, INT_MAX);
static std::uniform_int_distribution<unsigned> payload_dist(0U, 10U);
static constexpr std::size_t N {1024};

template <typename T>
void test_unequal()
{
    for (std::size_t i {}; i < N; ++i)
    {
        const auto lhs_int {dist(rng)};
        const auto rhs_int {dist(rng)};

        const T lhs {lhs_int};
        const T rhs {rhs_int};

        if (lhs_int < rhs_int)
        {
            BOOST_TEST(comparetotal(lhs, rhs));
        }
        else if (lhs_int > rhs_int)
        {
            BOOST_TEST(!comparetotal(lhs, rhs));
        }
    }
}

template <typename T>
void test_part_d12()
{
    for (std::size_t i {}; i < N / 2U; ++i)
    {
        const auto rhs_int {dist(rng)};

        const auto lhs {-std::numeric_limits<T>::quiet_NaN()};
        const T rhs {rhs_int};

        BOOST_TEST(comparetotal(lhs, rhs));
    }

    for (std::size_t i {}; i < N / 2U; ++i)
    {
        const auto lhs_int {dist(rng)};

        const T lhs {lhs_int};
        const auto rhs {std::numeric_limits<T>::quiet_NaN()};

        BOOST_TEST(comparetotal(lhs, rhs));
    }
}

template <typename T>
void test_part_d3()
{
    // d.3.i
    for (std::size_t i {}; i < N / 3; ++i)
    {
        const auto lhs_int {dist(rng)};
        const auto rhs_int {dist(rng)};

        const T lhs {lhs_int * -std::numeric_limits<T>::quiet_NaN()};
        const T rhs {rhs_int * std::numeric_limits<T>::quiet_NaN()};

        BOOST_TEST(comparetotal(lhs, rhs));
        BOOST_TEST(!comparetotal(rhs, lhs));
        BOOST_TEST(!comparetotal(rhs, rhs));
    }

    // d.3.ii
    for (std::size_t i {}; i < N / 3; ++i)
    {
        const auto rhs_int {dist(rng)};

        const T lhs {std::numeric_limits<T>::signaling_NaN()};
        const T rhs {rhs_int * std::numeric_limits<T>::quiet_NaN()};

        BOOST_TEST(comparetotal(lhs, rhs));
        BOOST_TEST(!comparetotal(rhs, lhs));
        BOOST_TEST(!comparetotal(rhs, rhs));

        const T neg_lhs {-std::numeric_limits<T>::signaling_NaN()};
        const T neg_rhs {rhs_int * -std::numeric_limits<T>::quiet_NaN()};

        BOOST_TEST(!comparetotal(neg_lhs, neg_rhs));
        BOOST_TEST(comparetotal(neg_rhs, neg_lhs));
    }

    // d.3.iii
    for (std::size_t i {}; i < N / 3; ++i)
    {
        const auto lhs_int {payload_dist(rng)};
        const auto rhs_int {payload_dist(rng)};

        const auto lhs_int_string {std::to_string(lhs_int)};
        const auto rhs_int_string {std::to_string(rhs_int)};

        const auto lhs {nan<T>(lhs_int_string.c_str())};
        const auto rhs {nan<T>(rhs_int_string.c_str())};

        if (lhs_int < rhs_int)
        {
            BOOST_TEST(comparetotal(lhs, rhs));
            BOOST_TEST(!comparetotal(rhs, lhs));
        }
        else if (lhs_int > rhs_int)
        {
            BOOST_TEST(!comparetotal(lhs, rhs));
            BOOST_TEST(comparetotal(rhs, lhs));
        }
        else
        {
            BOOST_TEST(!comparetotal(lhs, rhs));
            BOOST_TEST(!comparetotal(rhs, lhs));
        }
    }
}

int main()
{
    test_unequal<decimal32_t>();
    test_unequal<decimal64_t>();
    test_unequal<decimal128_t>();

    test_unequal<decimal_fast32_t>();
    test_unequal<decimal_fast64_t>();
    test_unequal<decimal_fast128_t>();

    test_part_d12<decimal32_t>();
    test_part_d12<decimal64_t>();
    test_part_d12<decimal128_t>();

    test_part_d12<decimal_fast32_t>();
    test_part_d12<decimal_fast64_t>();
    test_part_d12<decimal_fast128_t>();

    test_part_d3<decimal32_t>();
    test_part_d3<decimal64_t>();
    test_part_d3<decimal128_t>();

    test_part_d3<decimal_fast32_t>();
    test_part_d3<decimal_fast64_t>();
    test_part_d3<decimal_fast128_t>();

    return boost::report_errors();
}
