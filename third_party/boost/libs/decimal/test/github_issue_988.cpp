// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// See: https://github.com/boostorg/decimal/issues/988

#ifndef BOOST_DECIMAL_USE_MODULE
#include <boost/decimal.hpp>
#endif

#include <boost/core/lightweight_test.hpp>
#include <iostream>

#ifdef BOOST_DECIMAL_USE_MODULE
import boost.decimal;
#endif

using namespace boost::decimal;

constexpr std::uint64_t value {0xb0005af3107a3ff0};
constexpr decimal64_t decimal_value {from_bits(value)};


void test()
{
    // Regular to chars
    char buffer[256];
    const auto r = to_chars(buffer, buffer + sizeof(buffer), decimal_value);
    BOOST_TEST(r);
    *r.ptr = '\0';
    BOOST_TEST_CSTR_EQ(buffer, "-0.99999999999984");
}

void general_precision_test()
{
    char buffer[256];
    const auto r = to_chars(buffer, buffer + sizeof(buffer), decimal_value, chars_format::general, 6);
    BOOST_TEST(r);
    *r.ptr = '\0';
    BOOST_TEST_CSTR_EQ(buffer, "-1");
}

void fixed_precision_test()
{
    char buffer[256];
    const auto r = to_chars(buffer, buffer + sizeof(buffer), decimal_value, chars_format::fixed, 6);
    BOOST_TEST(r);
    *r.ptr = '\0';
    BOOST_TEST_CSTR_EQ(buffer, "-1.000000");
}

void scientific_precision_test()
{
    char buffer[256];
    const auto r = to_chars(buffer, buffer + sizeof(buffer), decimal_value, chars_format::scientific, 6);
    BOOST_TEST(r);
    *r.ptr = '\0';
    BOOST_TEST_CSTR_EQ(buffer, "-1.000000e+00");
}

int main()
{
    test();
    general_precision_test();
    fixed_precision_test();
    scientific_precision_test();

    return boost::report_errors();
}
