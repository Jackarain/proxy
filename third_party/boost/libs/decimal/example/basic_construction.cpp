// Copyright 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// This file demonstrates some of the very basic ways you con construct decimal types
// This includes: from integer; integer and exponent; integer exponent and sign; string

#include <boost/decimal/decimal32_t.hpp>    // For decimal32_t type
#include <boost/decimal/iostream.hpp>       // For <iostream> support of decimal types
#include <boost/decimal/cmath.hpp>          // For isnan and isinf
#include <string>
#include <iostream>

int main()
{
    using boost::decimal::decimal32_t;          // The decimal32_t type
    using boost::decimal::construction_sign;    // An enum class for specifying sign during certain instances of construction
    using boost::decimal::isinf;                // Analogous to std::isinf but for decimal types
    using boost::decimal::isnan;                // Analogous to std::isnan but for decimal types

    // Construction from an integer
    constexpr decimal32_t val_1 {100};

    // Construction from a signed integer and exponent
    constexpr decimal32_t val_2 {10, 1};

    // Construction from an unsigned integer, exponent, and sign
    // The sign enum is named construction_sign, and has two members: positive and negative
    constexpr decimal32_t val_3 {1U, 2, construction_sign::negative};

    std::cout << "Val_1: " << val_1 << '\n'
              << "Val_2: " << val_2 << '\n'
              << "Val_3: " << val_3 << '\n';

    if (val_1 == val_2 && val_2 == val_3 && val_1 == val_3)
    {
        std::cout << "All equal values" << std::endl;
    }

    // Demonstration of the overflow and underflow handling
    // A value that overflows constructs an infinity (which can be queried using isinf)
    // A value that underflows constructs a zero
    constexpr decimal32_t overflow_value {100, 10000};
    if (isinf(overflow_value))
    {
        std::cout << "Overflow constructs infinity" << std::endl;
    }

    constexpr decimal32_t underflow_value {100, -10000};
    constexpr decimal32_t zero {0};
    if (underflow_value == zero)
    {
        std::cout << "Underflow constructs zero" << std::endl;
    }

    // Construction of NANs can be done using numeric limits,
    // and checked using the normal isnan function
    constexpr decimal32_t non_finite_from_float {std::numeric_limits<decimal32_t>::quiet_NaN()};
    if (isnan(non_finite_from_float))
    {
        std::cout << "NaN constructs NaN" << std::endl;
    }

    // We can also construct both from a C-string (const char*) and from a std::string

    const char* c_string_value {"4.3e-02"};
    const decimal32_t from_c_string {c_string_value};
    const decimal32_t from_std_string {std::string(c_string_value)};

    if (from_c_string == from_std_string)
    {
        std::cout << "Values constructed from const char* and std::string are the same" << '\n';
    }

    // If we attempt construction for a string that cannot be converted into a decimal value,
    // the constructor will do 1 of 2 things:
    // 1) In a noexcept environment the constructor will return a quiet NaN
    // 2) Otherwise it will throw
    //
    // The exception environment is detected automatically,
    // or can be set by defining BOOST_DECIMAL_DISABLE_EXCEPTIONS

    const char* bad_string {"Junk_String"};

    #ifndef BOOST_DECIMAL_DISABLE_EXCEPTIONS

    try
    {
        const decimal32_t throwing_value {bad_string};
        std::cout << throwing_value << '\n';
    }
    catch (const std::runtime_error& e)
    {
        std::cout << e.what() << std::endl;
    }

    #else

    const decimal32_t nan_value {bad_string};
    if (isnan(nan_value))
    {
        std::cout << "Bad string construction has formed a NAN" << std::endl;
    }

    #endif
}
