// Copyright 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// This file demonstrates some of the basic numerical operations with the decimal types

#include <boost/decimal/decimal64_t.hpp>    // For type decimal64_t
#include <boost/decimal/cmath.hpp>          // For decimal overloads of <cmath> functions
#include <boost/decimal/iostream.hpp>       // For decimal support of <iostream> and <iomanip>
#include <iostream>
#include <iomanip>

int main()
{
    using boost::decimal::decimal64_t;      // Type decimal64_t

    constexpr decimal64_t a {"-5.123456891234567"};  // Constructs -5.123456 from string
    constexpr decimal64_t b {"3.123456891234567"};   // Constructs 3.123456 from string
    constexpr decimal64_t c {a + b};

    // Here we can see that the result is exact
    constexpr decimal64_t neg_two {-2};
    static_assert(c == neg_two, "Result should be exact");

    // We can use std::setprecision and std::numeric_limits in their usual ways
    std::cout << std::setprecision(std::numeric_limits<decimal64_t>::digits10)
              << "A: " << a << '\n'
              << "B: " << b << '\n'
              << "A + B: " << c << '\n';

    // The decimal library provides comprehensive implementations of the <cmath> functions
    // that you would expect to have with the builtin floating point types
    //
    // They are all located in namespace boost::decimal::,
    // as overloading namespace std is not allowed

    constexpr decimal64_t abs_c {boost::decimal::abs(c)};
    std::cout << "abs(A + B): " << abs_c << '\n';

    // All cmath functions are constexpr even if their std:: counterparts are not
    constexpr decimal64_t sqrt_two {boost::decimal::sqrt(abs_c)};

    // Value compute by N[Sqrt[2], 50] using Wolfram Alpha or Mathematica
    constexpr decimal64_t wa_sqrt_two {"1.4142135623730950488016887242096980785696718753769"};
    std::cout << "sqrt(abs(A + B)): " << sqrt_two << '\n'
              << "Wolfram Alpha sqrt(2): " << wa_sqrt_two << '\n';
}
