// Copyright 2026 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// See: https://github.com/boostorg/decimal/issues/1294

#include <boost/decimal.hpp>
#include <boost/core/lightweight_test.hpp>
#include <limits>

using namespace boost::decimal;

template <typename T>
void test()
{
    // Positive LHS, Positive RHS
    BOOST_TEST_EQ(std::numeric_limits<T>::infinity() * std::numeric_limits<T>::infinity(), std::numeric_limits<T>::infinity());
    BOOST_TEST_EQ(std::numeric_limits<T>::infinity() * T{1000}, std::numeric_limits<T>::infinity());
    BOOST_TEST_EQ(std::numeric_limits<T>::infinity() * 1000, std::numeric_limits<T>::infinity());
    BOOST_TEST_EQ(T{1000} * std::numeric_limits<T>::infinity(), std::numeric_limits<T>::infinity());
    BOOST_TEST_EQ(1000 * std::numeric_limits<T>::infinity(), std::numeric_limits<T>::infinity());
    BOOST_TEST(isnan(std::numeric_limits<T>::infinity() * std::numeric_limits<T>::quiet_NaN()));
    BOOST_TEST(isnan(std::numeric_limits<T>::quiet_NaN() * std::numeric_limits<T>::infinity()));

    // Positive LHS, Negative RHS
    BOOST_TEST_EQ(std::numeric_limits<T>::infinity() * -std::numeric_limits<T>::infinity(), -std::numeric_limits<T>::infinity());
    BOOST_TEST_EQ(std::numeric_limits<T>::infinity() * -T{1000}, -std::numeric_limits<T>::infinity());
    BOOST_TEST_EQ(std::numeric_limits<T>::infinity() * -1000, -std::numeric_limits<T>::infinity());
    BOOST_TEST_EQ(T{1000} * -std::numeric_limits<T>::infinity(), -std::numeric_limits<T>::infinity());
    BOOST_TEST_EQ(1000 * -std::numeric_limits<T>::infinity(), -std::numeric_limits<T>::infinity());
    BOOST_TEST(isnan(std::numeric_limits<T>::infinity() * -std::numeric_limits<T>::quiet_NaN()));
    BOOST_TEST(isnan(std::numeric_limits<T>::quiet_NaN() * -std::numeric_limits<T>::infinity()));

    // Negative RHS, positive LHS
    BOOST_TEST_EQ(-std::numeric_limits<T>::infinity() * std::numeric_limits<T>::infinity(), -std::numeric_limits<T>::infinity());
    BOOST_TEST_EQ(-std::numeric_limits<T>::infinity() * T{1000}, -std::numeric_limits<T>::infinity());
    BOOST_TEST_EQ(-std::numeric_limits<T>::infinity() * 1000, -std::numeric_limits<T>::infinity());
    BOOST_TEST_EQ(-T{1000} * std::numeric_limits<T>::infinity(), -std::numeric_limits<T>::infinity());
    BOOST_TEST_EQ(-1000 * std::numeric_limits<T>::infinity(), -std::numeric_limits<T>::infinity());
    BOOST_TEST(isnan(-std::numeric_limits<T>::infinity() * std::numeric_limits<T>::quiet_NaN()));
    BOOST_TEST(isnan(-std::numeric_limits<T>::quiet_NaN() * std::numeric_limits<T>::infinity()));

    // Negative RHS, Negative RHS
    BOOST_TEST_EQ(-std::numeric_limits<T>::infinity() * -std::numeric_limits<T>::infinity(), std::numeric_limits<T>::infinity());
    BOOST_TEST_EQ(-std::numeric_limits<T>::infinity() * -T{1000}, std::numeric_limits<T>::infinity());
    BOOST_TEST_EQ(-std::numeric_limits<T>::infinity() * -1000, std::numeric_limits<T>::infinity());
    BOOST_TEST_EQ(-T{1000} * -std::numeric_limits<T>::infinity(), std::numeric_limits<T>::infinity());
    BOOST_TEST_EQ(-1000 * -std::numeric_limits<T>::infinity(), std::numeric_limits<T>::infinity());
    BOOST_TEST(isnan(-std::numeric_limits<T>::infinity() * -std::numeric_limits<T>::quiet_NaN()));
    BOOST_TEST(isnan(-std::numeric_limits<T>::quiet_NaN() * -std::numeric_limits<T>::infinity()));

}

int main()
{
    test<decimal32_t>();
    test<decimal64_t>();
    test<decimal128_t>();

    test<decimal_fast32_t>();
    test<decimal_fast64_t>();
    test<decimal_fast128_t>();

    return boost::report_errors();
}
