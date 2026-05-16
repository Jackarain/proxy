// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/core/lightweight_test.hpp>
#include <boost/decimal/detail/fast_float/compute_float80_128.hpp>
#include <random>
#include <limits>
#include <cmath>

inline bool test_close(long double lhs, long double rhs)
{
    return std::abs(lhs - rhs) < std::numeric_limits<long double>::epsilon();
}

int main()
{
    // We need these to force runtime evaluation of tests for coverage
    std::mt19937_64 rng(42);
    std::uniform_int_distribution<int> dist(1, 1);

    bool success {false};
    std::int64_t q {0 * dist(rng)};
    std::uint64_t w {static_cast<std::uint64_t>(0 * dist(rng))};

    // Fast path
    auto res = boost::decimal::detail::fast_float::compute_float80_128(q, w, false, success);
    BOOST_TEST(success);
    BOOST_TEST(test_close(res, 0.0L));

    // Zero path
    q = 1000 * dist(rng);
    success = false;
    res = boost::decimal::detail::fast_float::compute_float80_128(q, w, true, success);
    BOOST_TEST(success);
    BOOST_TEST(test_close(res, -0.0L));

    // Huge Val path
    w = static_cast<std::uint64_t>(dist(rng));
    q = static_cast<std::int64_t>(100000);
    success = false;
    res = boost::decimal::detail::fast_float::compute_float80_128(q, w, false, success);
    BOOST_TEST(success);
    BOOST_TEST(test_close(res, HUGE_VALL) || std::isinf(res)); // Depending on the definition of HUGE_VALL

    return boost::report_errors();
}
