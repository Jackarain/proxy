// Copyright 2026 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// See: https://github.com/boostorg/decimal/issues/1319

#include <boost/decimal.hpp>
#include <boost/core/lightweight_test.hpp>
#include <limits>

using namespace boost::decimal;

template <typename T>
void test()
{
    constexpr T inf {std::numeric_limits<T>::infinity()};
    constexpr T snan {std::numeric_limits<T>::signaling_NaN()};
    constexpr T nan {std::numeric_limits<T>::quiet_NaN()};
    constexpr T finite {1000};

    // Snans should degrade to qnans
    BOOST_TEST(isnan(inf % snan) && !issignaling(inf % snan));
    BOOST_TEST(isnan(snan % inf) && !issignaling(snan % inf));
    BOOST_TEST(isnan(snan % finite) && !issignaling(snan % finite));
    BOOST_TEST(isnan(finite % snan) && !issignaling(finite % snan));
    BOOST_TEST(isnan(nan % finite));
    BOOST_TEST(isnan(finite % nan));
    BOOST_TEST(isnan(nan % snan) && !issignaling(nan % snan));
    BOOST_TEST(isnan(snan % nan) && !issignaling(snan % nan));
    BOOST_TEST_EQ(finite % inf, finite);
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
