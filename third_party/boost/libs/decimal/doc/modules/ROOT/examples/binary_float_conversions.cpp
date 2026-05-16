// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// This file demonstrates how to convert various types to decimal types and back,
// along with edge case handling

#include <boost/decimal/decimal32_t.hpp>        // For type decimal32_t
#include <boost/decimal/decimal64_t.hpp>        // For type decimal64_t
#include <boost/decimal/cmath.hpp>              // For decimal support of cmath functions
#include <boost/decimal/iostream.hpp>           // For decimal support of <iostream> and <iomanip>
#include <boost/decimal/numbers.hpp>            // For decimal support of <numbers>
#include <iostream>
#include <cmath>
#include <limits>

int main()
{
    using boost::decimal::decimal32_t;      // Type decimal32_t
    using boost::decimal::decimal64_t;      // Type decimal64_t

    // Non-finite values construct the equivalent non-finite value in binary floating point
    constexpr decimal64_t decimal_qnan {std::numeric_limits<decimal64_t>::quiet_NaN()};
    const double double_from_qnan {static_cast<double>(decimal_qnan)};

    // Note here that we must use boost::decimal::isnan for decimal types,
    // as it is illegal to overload std::isnan
    if (boost::decimal::isnan(decimal_qnan) && std::isnan(double_from_qnan))
    {
        std::cout << "Decimal QNAN converts to double QNAN\n";
    }

    constexpr decimal64_t decimal_inf {std::numeric_limits<decimal64_t>::infinity()};
    const double double_from_inf {static_cast<double>(decimal_inf)};

    // Same as the above but with INF instead of NAN
    if (boost::decimal::isinf(decimal_inf) && std::isinf(double_from_inf))
    {
        std::cout << "Decimal INFINITY converts to double INFINITY\n";
    }

    // For finite values we make a best effort approach to covert to double
    // We are able to decompose the decimal floating point value into a sign, significand, and exponent.
    // From there we use the methods outline in Daniel Lemire's "Number Parsing at a Gigabyte a Second",
    // to construct the binary floating point value.
    // See: https://arxiv.org/pdf/2101.11408

    // Construct the decimal64_t version of pi using our pre-computed constants from <boost/decimal/numbers.hpp>
    constexpr decimal64_t decimal_pi {boost::decimal::numbers::pi_v<decimal64_t>};
    const double double_from_pi {static_cast<double>(decimal_pi)};

    std::cout << std::setprecision(std::numeric_limits<decimal64_t>::digits10)
              << "decimal64_t pi: " << decimal_pi << '\n'
              << "     double pi: " << double_from_pi << '\n';

    // To construct a decimal64_t from double we use the methods described in "Ryu: fast float-to-string conversion"
    // See: https://dl.acm.org/doi/10.1145/3192366.3192369
    // This paper shows how to decompose a double into it's sign, significand, and exponent
    // Once we have those components we can use the normal constructors of the decimal types to construct
    // Since we are using the normal constructors here,
    // any construction from this conversion is subject to the current rounding mode
    // Such as with a lossy conversion like shown (double -> decimal32_t)

    const decimal64_t decimal_from_double {static_cast<decimal64_t>(double_from_pi)};
    const decimal32_t lossy_decimal_from_double {static_cast<decimal32_t>(double_from_pi)};

    std::cout << "  Converted pi: " << decimal_from_double << '\n'
              << "decimal32_t pi: " << lossy_decimal_from_double << '\n';


    // Other than what has already been shown,
    // there are no other ways in the library to convert between decimal types and binary floating point types
    // The reason for this is to discourage their use.
    //
    // You can use intermediate representations like strings if you want to make these conversions,
    // and want to be sure about what the resulting value will be

    return 0;
}
