// Copyright 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/decimal/decimal32_t.hpp>
#include <boost/decimal/decimal64_t.hpp>
#include <boost/decimal/decimal128_t.hpp>
#include <boost/decimal/decimal_fast32_t.hpp>
#include <boost/decimal/decimal_fast64_t.hpp>
#include <boost/decimal/decimal_fast128_t.hpp>
#include <boost/decimal/dpd_conversion.hpp>
#include <boost/decimal/iostream.hpp>
#include <boost/core/lightweight_test.hpp>
#include <random>

using namespace boost::decimal;

template <typename T>
T roundtrip(T val)
{
    const auto bits {to_dpd<T>(val)};
    return from_dpd<T>(bits);
}

template <typename T>
void test()
{
    std::mt19937_64 rng(42);
    std::uniform_int_distribution<std::int64_t> dist(std::numeric_limits<std::int64_t>::min(),
                                                     std::numeric_limits<std::int64_t>::max());

    for (std::size_t i {}; i < 1024; ++i)
    {
        const T val {dist(rng)};
        const T return_val {roundtrip(val)};
        BOOST_TEST_EQ(val, return_val);
    }

    // Non-finite values
    BOOST_TEST(isinf(roundtrip(std::numeric_limits<T>::infinity())));
    BOOST_TEST(isinf(roundtrip(-std::numeric_limits<T>::infinity())));
    BOOST_TEST(isnan(roundtrip(std::numeric_limits<T>::quiet_NaN())));
    BOOST_TEST(isnan(roundtrip(-std::numeric_limits<T>::quiet_NaN())));
    BOOST_TEST(isnan(roundtrip(std::numeric_limits<T>::signaling_NaN())));
    BOOST_TEST(isnan(roundtrip(-std::numeric_limits<T>::signaling_NaN())));
}

template <typename T>
void test_float_range()
{
    using float_type = std::conditional_t<std::is_same<T, decimal32_t>::value ||
                                          std::is_same<T, decimal_fast32_t>::value, float, double>;

    std::mt19937_64 rng(42);
    std::uniform_real_distribution<float_type> dist(std::numeric_limits<float_type>::min(),
                                                    std::numeric_limits<float_type>::max());

    for (std::size_t i {}; i < 1024; ++i)
    {
        const T val {dist(rng)};
        const T return_val {roundtrip(val)};
        BOOST_TEST_EQ(val, return_val);
    }
}

// See: https://github.com/boostorg/decimal/issues/1081
template <typename T>
void test_git_issue_1081()
{
    std::mt19937_64 rng(42);
    std::uniform_int_distribution<std::int64_t> dist(std::numeric_limits<std::int64_t>::min(),
                                                     std::numeric_limits<std::int64_t>::max());

    for (std::size_t i {}; i < 1024; ++i)
    {
        const T val {dist(rng), std::numeric_limits<T>::min_exponent};
        const T return_val {roundtrip(val)};
        BOOST_TEST_EQ(val, return_val);
    }

    for (std::size_t i {}; i < 1024; ++i)
    {
        const T val {dist(rng), std::numeric_limits<T>::max_exponent - std::numeric_limits<std::int64_t>::digits10 - 1};
        const T return_val {roundtrip(val)};
        BOOST_TEST_EQ(val, return_val);
    }

    BOOST_TEST(roundtrip(std::numeric_limits<T>::denorm_min()));
}

int main()
{
    test<decimal32_t>();
    test<decimal_fast32_t>();

    test_float_range<decimal32_t>();
    test_float_range<decimal_fast32_t>();

    test<decimal64_t>();
    test<decimal_fast64_t>();

    test_float_range<decimal64_t>();
    test_float_range<decimal_fast64_t>();

    test<decimal128_t>();
    test<decimal_fast128_t>();

    test_float_range<decimal128_t>();
    test_float_range<decimal_fast128_t>();

    test_git_issue_1081<decimal32_t>();
    test_git_issue_1081<decimal64_t>();
    test_git_issue_1081<decimal128_t>();

    return boost::report_errors();
}
