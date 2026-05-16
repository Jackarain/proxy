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
#include <limits>
#include <cstdint>

int main()
{
    using boost::decimal::decimal32_t;      // Type decimal32_t
    using boost::decimal::decimal64_t;      // Type decimal64_t

    // Non-finite values construct std::numeric_limits<TargetIntegerType>::max()
    constexpr decimal64_t decimal_qnan {std::numeric_limits<decimal64_t>::quiet_NaN()};
    const std::uint32_t int_from_nan {static_cast<std::uint32_t>(decimal_qnan)};

    // Note here that we must use boost::decimal::isnan for decimal types,
    // as it is illegal to overload std::isnan
    if (boost::decimal::isnan(decimal_qnan) && int_from_nan == std::numeric_limits<std::uint32_t>::max())
    {
        std::cout << "Decimal QNAN converts to Integer Max\n";
    }

    // Same thing happens with decimal infinities since integers don't have an infinity
    constexpr decimal32_t decimal_inf {std::numeric_limits<decimal32_t>::infinity()};
    const std::uint64_t int_from_inf {static_cast<std::uint64_t>(decimal_inf)};

    // Same as the above but with INF instead of NAN
    if (boost::decimal::isinf(decimal_inf) && int_from_inf == std::numeric_limits<std::uint64_t>::max())
    {
        std::cout << "Decimal INF converts to Integer Max\n";
    }

    // For finite values the construction of the resulting integer matches the behavior
    // you are familiar with from binary floating point to integer conversions.
    // Namely, the result will only have the integer component of the decimal

    // Construct the decimal64_t version of pi using our pre-computed constants from <boost/decimal/numbers.hpp>
    constexpr decimal64_t decimal_pi {boost::decimal::numbers::pi_v<decimal64_t>};
    const std::uint32_t int_from_pi {static_cast<std::uint32_t>(decimal_pi)};

    std::cout << std::setprecision(std::numeric_limits<decimal64_t>::digits10)
              << "  decimal64_t pi: " << decimal_pi << '\n'
              << "std::uint32_t pi: " << int_from_pi << "\n\n";

    // Constructing a decimal value from an integer is lossless until
    // the number of digits in the integer exceeds the precision of the decimal type

    std::cout << "Conversions will be lossless\n"
              << "  decimal64_t digits10: " << std::numeric_limits<decimal64_t>::digits10 << "\n"
              << "std::uint32_t digits10: " << std::numeric_limits<std::uint32_t>::digits10 << "\n";

    constexpr decimal64_t decimal_from_u32_max {std::numeric_limits<std::uint32_t>::max()};
    std::cout << "   std::uint32_t max: " << std::numeric_limits<std::uint32_t>::max() << "\n"
              << "decimal64_t from max: " << decimal_from_u32_max << "\n\n";

    // In the construction of lossy values the rounding will be handled according to
    // the current global rounding mode.

    std::cout << "Conversions will be lossy\n"
          << "  decimal32_t digits10: " << std::numeric_limits<decimal32_t>::digits10 << "\n"
          << "std::uint64_t digits10: " << std::numeric_limits<std::uint64_t>::digits10 << "\n";

    constexpr decimal32_t decimal_from_u64_max {std::numeric_limits<std::uint64_t>::max()};
    std::cout << "   std::uint64_t max: " << std::numeric_limits<std::uint64_t>::max() << "\n"
              << "decimal32_t from max: " << decimal_from_u64_max << '\n';

    return 0;
}
