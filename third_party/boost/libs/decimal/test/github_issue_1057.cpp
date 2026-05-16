// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// See: https://github.com/boostorg/decimal/issues/1057

#ifndef BOOST_DECIMAL_USE_MODULE
#include <boost/decimal.hpp>
#endif

#include <boost/core/lightweight_test.hpp>
#include <iostream>

#ifdef BOOST_DECIMAL_USE_MODULE
import boost.decimal;
#endif

template <typename DecimalType>
void convert_leading_plus_by_strtod()
{
    DecimalType x = boost::decimal::strtod<DecimalType>("+0.1", nullptr);
    BOOST_TEST_EQ(x, DecimalType(1, -1));
}

template <typename DecimalType>
void convert_leading_space_by_strtod()
{
    DecimalType x = boost::decimal::strtod<DecimalType>(" \t 0.1", nullptr);
    BOOST_TEST_EQ(x, DecimalType(1, -1));
}

template <typename DecimalType>
void test_both()
{
    DecimalType x = boost::decimal::strtod<DecimalType>(" \n \t +0.1", nullptr);
    BOOST_TEST_EQ(x, DecimalType(1, -1));

    x = boost::decimal::strtod<DecimalType>(" \n \t -0.2", nullptr);
    BOOST_TEST_EQ(x, -DecimalType(2, -1));
}

template <typename DecimalType>
void test_segfault()
{
    DecimalType x = boost::decimal::strtod<DecimalType>(" \n \t +", nullptr);
    BOOST_TEST(boost::decimal::isnan(x));

    x = boost::decimal::strtod<DecimalType>(" \n \t", nullptr);
    BOOST_TEST(boost::decimal::isnan(x));
}

int main()
{
    using namespace boost::decimal;

    convert_leading_plus_by_strtod<decimal32_t>();
    convert_leading_plus_by_strtod<decimal64_t>();
    convert_leading_plus_by_strtod<decimal128_t>();
    convert_leading_plus_by_strtod<decimal_fast32_t>();
    convert_leading_plus_by_strtod<decimal_fast64_t>();
    convert_leading_plus_by_strtod<decimal_fast128_t>();

    convert_leading_space_by_strtod<decimal32_t>();
    convert_leading_space_by_strtod<decimal64_t>();
    convert_leading_space_by_strtod<decimal128_t>();
    convert_leading_space_by_strtod<decimal_fast32_t>();
    convert_leading_space_by_strtod<decimal_fast64_t>();
    convert_leading_space_by_strtod<decimal_fast128_t>();

    test_both<decimal32_t>();
    test_both<decimal64_t>();
    test_both<decimal128_t>();
    test_both<decimal_fast32_t>();
    test_both<decimal_fast64_t>();
    test_both<decimal_fast128_t>();

    test_segfault<decimal32_t>();
    test_segfault<decimal64_t>();
    test_segfault<decimal128_t>();
    test_segfault<decimal_fast32_t>();
    test_segfault<decimal_fast64_t>();
    test_segfault<decimal_fast128_t>();

    return boost::report_errors();
}
