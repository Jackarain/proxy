// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// See: https://github.com/boostorg/decimal/issues/1107

#include <boost/decimal.hpp>
#include <boost/core/lightweight_test.hpp>
#include <limits>
#include <random>

using namespace boost::decimal;

static std::mt19937_64 rng(42);
static std::uniform_int_distribution<int> dist(1, 10);

// 7.2.b
template <typename T>
void test_mul()
{
    const T a {dist(rng) * 0};
    const T b {dist(rng) * std::numeric_limits<T>::infinity()};

    BOOST_TEST(a == 0);
    BOOST_TEST(isinf(b));

    BOOST_TEST(isnan(a * b));
    BOOST_TEST(isnan(b * a));
}

// 7.2.d
template <typename T>
void test_add_sub()
{
    const T a {dist(rng) * std::numeric_limits<T>::infinity()};
    const T b {dist(rng) * -std::numeric_limits<T>::infinity()};

    BOOST_TEST(!signbit(a));
    BOOST_TEST(signbit(b));

    BOOST_TEST(isnan(b + a)); // -inf + inf
    BOOST_TEST(isnan(a + b)); //  inf - inf
    BOOST_TEST(isnan(b - b)); // -inf + inf
    BOOST_TEST(isnan(a - a)); //  inf - inf
}

// 7.2.e
template <typename T>
void test_div()
{
    const T a {dist(rng) * 0};
    const T b {dist(rng) * std::numeric_limits<T>::infinity()};

    BOOST_TEST(a == 0);
    BOOST_TEST(isinf(b));

    BOOST_TEST(isnan(a / a));
    BOOST_TEST(isnan(b / b));
}

// 7.2.f
template <typename T>
void test_remainder()
{
    const T a {dist(rng) * 0};
    const T b {dist(rng) * std::numeric_limits<T>::infinity()};
    const T c {dist(rng)};

    BOOST_TEST(isnan(remainder(b, c)));
    BOOST_TEST(isnan(remainder(c, a)));
}

// 7.2.g
template <typename T>
void test_sqrt()
{
    const T val {-dist(rng)};
    const auto sqrt_val {sqrt(val)};
    BOOST_TEST(isnan(sqrt_val));
}

int main()
{
    test_add_sub<decimal32_t>();
    test_add_sub<decimal64_t>();
    test_add_sub<decimal128_t>();
    test_add_sub<decimal_fast32_t>();
    test_add_sub<decimal_fast64_t>();
    test_add_sub<decimal_fast128_t>();

    test_mul<decimal32_t>();
    test_mul<decimal64_t>();
    test_mul<decimal128_t>();
    test_mul<decimal_fast32_t>();
    test_mul<decimal_fast64_t>();
    test_mul<decimal_fast128_t>();

    test_div<decimal32_t>();
    test_div<decimal64_t>();
    test_div<decimal128_t>();
    test_div<decimal_fast32_t>();
    test_div<decimal_fast64_t>();
    test_div<decimal_fast128_t>();

    test_remainder<decimal32_t>();
    test_remainder<decimal64_t>();
    test_remainder<decimal128_t>();
    test_remainder<decimal_fast32_t>();
    test_remainder<decimal_fast64_t>();
    test_remainder<decimal_fast128_t>();

    test_sqrt<decimal32_t>();
    test_sqrt<decimal64_t>();
    test_sqrt<decimal128_t>();
    test_sqrt<decimal_fast32_t>();
    test_sqrt<decimal_fast64_t>();
    test_sqrt<decimal_fast128_t>();

    return boost::report_errors();
}
