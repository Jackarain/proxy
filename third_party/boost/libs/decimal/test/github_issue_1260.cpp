// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// See: https://github.com/boostorg/decimal/issues/1260

#include <boost/decimal.hpp>
#include <boost/core/lightweight_test.hpp>
#include <iostream>
#include <iomanip>

template <typename T>
void test()
{
    const T lhs {"1E34"};
    const T rhs {"-0.51"};
    const T res {"9999999999999999999999999999999999"};

    const T add_val {lhs + rhs};
    BOOST_TEST_EQ(add_val, res);

    const T sub_val {lhs - boost::decimal::abs(rhs)};
    BOOST_TEST_EQ(sub_val, res);
}

template <typename T>
void test2()
{
    const T lhs {"1E34"};
    const T rhs {"-0.501"};
    const T res {"9999999999999999999999999999999999"};

    const T add_val {lhs + rhs};
    BOOST_TEST_EQ(add_val, res);

    const T sub_val {lhs - boost::decimal::abs(rhs)};
    BOOST_TEST_EQ(sub_val, res);
}

template <typename T>
void test3()
{
    const T lhs {"1E34"};
    const T rhs {"-0.5001"};
    const T res {"9999999999999999999999999999999999"};

    const T add_val {lhs + rhs};
    BOOST_TEST_EQ(add_val, res);

    const T sub_val {lhs - boost::decimal::abs(rhs)};
    BOOST_TEST_EQ(sub_val, res);
}

template <typename T>
void test4()
{
    const T lhs {"1E34"};
    const T rhs {"-0.50001"};
    const T res {"9999999999999999999999999999999999"};

    const T add_val {lhs + rhs};
    BOOST_TEST_EQ(add_val, res);

    const T sub_val {lhs - boost::decimal::abs(rhs)};
    BOOST_TEST_EQ(sub_val, res);
}

template <typename T>
void test5()
{
    const T lhs {"5"};
    const T rhs {"-3"};
    const T res {"2"};

    const T add_val {lhs + rhs};
    BOOST_TEST_EQ(add_val, res);
}

template <typename T>
void test6()
{
    // Only an issue on 32-bit platforms
    const T lhs {"10000e+9"};
    const T rhs {"7000"};
    const T res {"10000000007000"};

    const T add_val {lhs + rhs};
    BOOST_TEST_EQ(add_val, res);
}

int main()
{
    std::cerr << std::setprecision(std::numeric_limits<boost::decimal::decimal128_t>::max_digits10);

    test<boost::decimal::decimal128_t>();
    test<boost::decimal::decimal_fast128_t>();

    test2<boost::decimal::decimal128_t>();
    test2<boost::decimal::decimal_fast128_t>();

    test3<boost::decimal::decimal128_t>();
    test3<boost::decimal::decimal_fast128_t>();

    test4<boost::decimal::decimal128_t>();
    test4<boost::decimal::decimal_fast128_t>();

    test5<boost::decimal::decimal128_t>();
    test5<boost::decimal::decimal_fast128_t>();

    test6<boost::decimal::decimal128_t>();
    test6<boost::decimal::decimal_fast128_t>();

    return boost::report_errors();
}
