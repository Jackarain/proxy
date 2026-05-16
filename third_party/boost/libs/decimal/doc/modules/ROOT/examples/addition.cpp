// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// This example shows the difference in the results of repeated addition

#include <boost/decimal/decimal32_t.hpp>    // For type decimal32_t and std::numeric_limits support
#include <boost/decimal/iostream.hpp>       // For decimal support to <iostream>
#include <iostream>
#include <limits>

int main()
{
    using boost::decimal::decimal32_t;

    constexpr decimal32_t decimal_one_tenth {"0.1"}; // Construct constant 0.1 from string for lossless conversion
    constexpr float float_one_tenth {0.1f};              // Construct floating point constant from literal

    decimal32_t decimal_value {};   // Construct decimal 0 to start from
    float float_value {};           // Construct float 0 to start from

    // We now add 0.1 1000 times which should result exactly in 100
    // What we actually find is that the decimal32_t value does result in exactly 100
    // With type float the result is not 100 due to inexact representation
    for (int i {}; i < 1000; ++i)
    {
        decimal_value += decimal_one_tenth; // Decimal types support compound arithmetic as expected
        float_value += float_one_tenth;
    }

    // Each of the decimal types has complete support for std::numeric_limits,
    // which we leverage here with set precision to show any fractional part of the number (if applicable)
    std::cout << std::setprecision(std::numeric_limits<decimal32_t>::digits10)
              << "Decimal Result: " << decimal_value << "\n"
              << std::setprecision(std::numeric_limits<float>::digits10)
              << "  Float Result: " << float_value << std::endl;
}
