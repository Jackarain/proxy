// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

// Use compile-time rounding mode change so the test run on all platforms
#define BOOST_DECIMAL_FE_DEC_DOWNWARD

#include <boost/decimal.hpp>
#include <boost/core/lightweight_test.hpp>
#include <functional>
#include <iostream>
#include <iomanip>

template <typename T, typename Func>
void test(const char* lhs_str, const char* rhs_str, const char* result_str, Func f)
{
    const T lhs {lhs_str};
    const T rhs {rhs_str};
    const T result {result_str};

    const T func_result {f(lhs, rhs)};

    BOOST_TEST_EQ(func_result, result);
}

template <typename T>
void test_add(const char* lhs, const char* rhs, const char* result)
{
    std::cerr << std::setprecision(std::numeric_limits<T>::max_digits10);
    test<T>(lhs, rhs, result, std::plus<>());
    test<T>(rhs, lhs, result, std::plus<>());
}

template <typename T>
void test_sub(const char* lhs, const char* rhs, const char* result)
{
    std::cerr << std::setprecision(std::numeric_limits<T>::max_digits10);
    test<T>(lhs, rhs, result, std::minus<>());
}

int main()
{
    using namespace boost::decimal;

    test_add<decimal64_t>("1e+2", "-1e-383", "99.99999999999999");
    test_add<decimal64_t>("1e+1", "-1e-383", "9.999999999999999");

    test_add<decimal_fast64_t>("1e+2", "-1e-383", "99.99999999999999");
    test_add<decimal_fast64_t>("1e+1", "-1e-383", "9.999999999999999");

    test_sub<decimal64_t>("1e+2", "1e-383", "99.99999999999999");
    test_sub<decimal64_t>("1e+1", "1e-383", "9.999999999999999");

    test_sub<decimal_fast64_t>("1e+2", "1e-383", "99.99999999999999");
    test_sub<decimal_fast64_t>("1e+1", "1e-383", "9.999999999999999");

    test_add<decimal32_t>("1e+2", "-1e-20", "99.99999");
    test_add<decimal32_t>("1e+1", "-1e-20", "9.999999");

    test_add<decimal_fast32_t>("1e+2", "-1e-20", "99.99999");
    test_add<decimal_fast32_t>("1e+1", "-1e-20", "9.999999");

    test_sub<decimal32_t>("1e+2", "1e-20", "99.99999");
    test_sub<decimal32_t>("1e+1", "1e-20", "9.999999");

    test_sub<decimal_fast32_t>("1e+2", "1e-20", "99.99999");
    test_sub<decimal_fast32_t>("1e+1", "1e-20", "9.999999");

    test_add<decimal128_t>("1e+2", "-1e-383", "99.99999999999999999999999999999999");
    test_add<decimal128_t>("1e+1", "-1e-383", "9.999999999999999999999999999999999");

    test_add<decimal_fast128_t>("1e+2", "-1e-383", "99.99999999999999999999999999999999");
    test_add<decimal_fast128_t>("1e+1", "-1e-383", "9.999999999999999999999999999999999");

    test_sub<decimal128_t>("1e+2", "1e-383", "99.99999999999999999999999999999999");
    test_sub<decimal128_t>("1e+1", "1e-383", "9.999999999999999999999999999999999");

    test_sub<decimal_fast128_t>("1e+2", "1e-383", "99.99999999999999999999999999999999");
    test_sub<decimal_fast128_t>("1e+1", "1e-383", "9.999999999999999999999999999999999");

    test_add<decimal32_t>("9.999999e20", "0", "9.999999e20");
    test_add<decimal_fast32_t>("9.999999e20", "0", "9.999999e20");

    test_sub<decimal32_t>("9.999999e20", "0", "9.999999e20");
    test_sub<decimal_fast32_t>("9.999999e20", "0", "9.999999e20");

    test_add<decimal64_t>("9.999999999999999e200", "0", "9.999999999999999e200");
    test_add<decimal_fast64_t>("9.999999999999999e200", "0", "9.999999999999999e200");

    test_sub<decimal64_t>("9.999999999999999e200", "0", "9.999999999999999e200");
    test_sub<decimal_fast64_t>("9.999999999999999e200", "0", "9.999999999999999e200");

    test_add<decimal128_t>("9.999999999999999e200", "0", "9.999999999999999e200");
    test_add<decimal_fast128_t>("9.999999999999999e200", "0", "9.999999999999999e200");

    test_sub<decimal128_t>("9.999999999999999e200", "0", "9.999999999999999e200");
    test_sub<decimal_fast128_t>("9.999999999999999e200", "0", "9.999999999999999e200");

    return boost::report_errors();
}
