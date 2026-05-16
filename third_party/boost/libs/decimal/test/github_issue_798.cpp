//  (C) Copyright Matt Borland 2025.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_DECIMAL_USE_MODULE
#include <boost/decimal.hpp>
#endif

#include <boost/core/lightweight_test.hpp>
#include <iomanip>
#include <limits>
#include <random>

#ifdef BOOST_DECIMAL_USE_MODULE
import boost.decimal;
#endif

template <typename T>
void test_zero()
{
    std::mt19937_64 rng(42);
    std::uniform_int_distribution<int> dist(1, 1);

    const T zero {0};
    const T one {dist(rng)};

    const auto next_after_zero {boost::decimal::nextafter(zero, one)};

    BOOST_TEST_GT(next_after_zero, zero);
    BOOST_TEST_LT(next_after_zero, zero + 2*std::numeric_limits<T>::min());

    const auto two_next_after_zero {boost::decimal::nextafter(next_after_zero, one)};
    BOOST_TEST_GT(two_next_after_zero, next_after_zero);
    BOOST_TEST_LT(two_next_after_zero, zero + 3*std::numeric_limits<T>::min());
}

template <typename T>
void test_eps()
{
    constexpr T eps {std::numeric_limits<T>::epsilon()};
    constexpr T one {1};

    const auto next_after_eps {boost::decimal::nextafter(eps, one)};

    BOOST_TEST_GT(next_after_eps, eps);
    BOOST_TEST_LT(next_after_eps, eps + 2*std::numeric_limits<T>::epsilon());
}

template <typename T>
void test_one()
{
    constexpr T one {1};
    constexpr T two {2};

    const auto next_after_one {boost::decimal::nextafter(one, two)};

    BOOST_TEST_GT(next_after_one, one);
    BOOST_TEST_LT(next_after_one, one + 2*std::numeric_limits<T>::epsilon());
}

template <typename T>
void test_onek()
{
    constexpr T onek {1024};
    constexpr T twok {2048};

    const auto next_after_onek {boost::decimal::nextafter(onek, twok)};

    BOOST_TEST_GT(next_after_onek, onek);
    BOOST_TEST_LT(next_after_onek, twok);
}

template <typename T>
void test_min()
{
    std::mt19937_64 rng(42);
    std::uniform_int_distribution<int> dist(1, 1);

    const T min_val {std::numeric_limits<T>::denorm_min()};
    const T one {dist(rng)};

    const auto next_after_min {boost::decimal::nextafter(min_val, one)};

    BOOST_TEST_GT(next_after_min, min_val);
    BOOST_TEST_LT(next_after_min, min_val + 3*std::numeric_limits<T>::min());

    const auto two_next_after_zero {boost::decimal::nextafter(next_after_min, one)};
    BOOST_TEST_GT(two_next_after_zero, next_after_min);
    BOOST_TEST_LT(two_next_after_zero, min_val + 4*std::numeric_limits<T>::min());
}

int main()
{
    using namespace boost::decimal;

    test_zero<decimal32_t>();
    test_zero<decimal_fast32_t>();
    test_zero<decimal64_t>();
    test_zero<decimal_fast64_t>();
    test_zero<decimal128_t>();
    test_zero<decimal_fast128_t>();

    test_eps<decimal32_t>();
    test_eps<decimal_fast32_t>();
    test_eps<decimal64_t>();
    test_eps<decimal_fast64_t>();
    test_eps<decimal128_t>();
    test_eps<decimal_fast128_t>();

    test_one<decimal32_t>();
    test_one<decimal_fast32_t>();
    test_one<decimal64_t>();
    test_one<decimal_fast64_t>();
    test_one<decimal128_t>();
    test_one<decimal_fast128_t>();

    test_onek<decimal32_t>();
    test_onek<decimal_fast32_t>();
    test_onek<decimal64_t>();
    test_onek<decimal_fast64_t>();
    test_onek<decimal128_t>();
    test_onek<decimal_fast128_t>();

    test_min<decimal32_t>();
    test_min<decimal_fast32_t>();
    test_min<decimal64_t>();
    test_min<decimal_fast64_t>();
    test_min<decimal128_t>();
    test_min<decimal_fast128_t>();

    return boost::report_errors();
}
