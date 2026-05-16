// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/decimal.hpp>
#include <boost/core/lightweight_test.hpp>
#include <string>

using namespace boost::decimal;

template <typename T>
void test_trivial()
{
    const auto str = "42";
    const T str_val {str};
    const T int_val {42};
    BOOST_TEST_EQ(str_val, int_val);
    BOOST_TEST_EQ(T{std::string(str)}, int_val);

    // We allow plus signs here by popular demand
    const auto str2 = "+1.2e+3";
    const T str2_val {str2};
    const T int2_val {12, 2};
    BOOST_TEST_EQ(str2_val, int2_val);
    BOOST_TEST_EQ(T{std::string(str2)}, int2_val);

    const auto str3 = "-1.2E003";
    const T str3_val {str3};
    BOOST_TEST_EQ(str3_val, -str2_val);
    BOOST_TEST_EQ(T{std::string(str3)}, str3_val);
}

#ifndef BOOST_DECIMAL_DISABLE_EXCEPTIONS

template <typename T>
void test_invalid()
{
    BOOST_TEST_THROWS(T("orange"), std::runtime_error);
    BOOST_TEST_THROWS(T(nullptr), std::runtime_error);
    BOOST_TEST_THROWS(T(""), std::runtime_error);
    BOOST_TEST_THROWS(T(std::string{""}), std::runtime_error);
}

#else

template <typename T>
void test_invalid()
{
    BOOST_TEST(isnan(T("orange")));
    BOOST_TEST(isnan(T(nullptr)));
    BOOST_TEST(isnan(T("")));
    BOOST_TEST(isnan(T(std::string{""})));
}

#endif

template <typename T>
void test_nonfinite()
{
    const auto nan_str = "nan";
    const T nan_val {nan_str};
    BOOST_TEST(isnan(nan_val));
    BOOST_TEST(!signbit(nan_val));

    const auto inf_str = "inf";
    const T inf_val {inf_str};
    BOOST_TEST(isinf(inf_val));
    BOOST_TEST(!signbit(inf_val));

    const auto neg_inf_str = "-inf";
    const T neg_inf_val {neg_inf_str};
    BOOST_TEST(isinf(neg_inf_val));
    BOOST_TEST(signbit(neg_inf_val));
}

int main()
{
    test_trivial<decimal32_t>();
    test_nonfinite<decimal32_t>();

    test_trivial<decimal64_t>();
    test_nonfinite<decimal64_t>();

    test_trivial<decimal128_t>();
    test_nonfinite<decimal128_t>();

    test_trivial<decimal_fast32_t>();
    test_nonfinite<decimal_fast32_t>();

    test_trivial<decimal_fast64_t>();
    test_nonfinite<decimal_fast64_t>();

    test_trivial<decimal_fast128_t>();
    test_nonfinite<decimal_fast128_t>();

    test_invalid<decimal32_t>();
    test_invalid<decimal64_t>();
    test_invalid<decimal128_t>();
    test_invalid<decimal_fast32_t>();
    test_invalid<decimal_fast64_t>();
    test_invalid<decimal_fast128_t>();

    return boost::report_errors();
}
