// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// This example demonstrates usage and formatting of decimal types with fmt

#include <boost/decimal/decimal32_t.hpp>    // For type decimal32_t
#include <boost/decimal/decimal64_t.hpp>    // For type decimal64_t
#include <boost/decimal/fmt_format.hpp>     // For {fmt} support
#include <iostream>

#if defined(BOOST_DECIMAL_HAS_FMTLIB_SUPPORT) && defined(BOOST_DECIMAL_TEST_FMT)

int main()
{
    constexpr boost::decimal::decimal64_t val1 {"3.14"};
    constexpr boost::decimal::decimal32_t val2 {"3.141"};

    // The easiest is no specification which is general format
    // Given these values they will print in fixed format
    std::cout << "Default Format:\n";
    std::cout << fmt::format("{}", val1) << '\n';
    std::cout << fmt::format("{}", val2) << "\n\n";

    // Next we can add a type modifier to get scientific formatting
    std::cout << "Scientific Format:\n";
    std::cout << fmt::format("{:e}", val1) << '\n';
    std::cout << fmt::format("{:e}", val2) << "\n\n";

    // Next we can add a type modifier to get scientific formatting
    // Here this gives one digit of precision rounded according to current rounding mode
    std::cout << "Scientific Format with Specified Precision:\n";
    std::cout << fmt::format("{:.1e}", val1) << '\n';
    std::cout << fmt::format("{:.1e}", val2) << "\n\n";

    // Width specifies the minimum field width
    // By default, values are right-aligned with space fill
    std::cout << "Width with Default Alignment (right-align, space fill):\n";
    std::cout << fmt::format("[{:12}]", val1) << '\n';
    std::cout << fmt::format("[{:12.3e}]", val2) << "\n\n";

    // Alignment specifiers: < (left), > (right), ^ (center)
    std::cout << "Alignment Specifiers:\n";
    std::cout << fmt::format("[{:<12}]", val1) << " (left)\n";
    std::cout << fmt::format("[{:>12}]", val1) << " (right)\n";
    std::cout << fmt::format("[{:^12}]", val1) << " (center)\n\n";

    // Fill character can be any character placed before the alignment specifier
    std::cout << "Custom Fill Characters:\n";
    std::cout << fmt::format("[{:*<12}]", val1) << " (left, * fill)\n";
    std::cout << fmt::format("[{:->12}]", val1) << " (right, - fill)\n";
    std::cout << fmt::format("[{:=^12}]", val1) << " (center, = fill)\n\n";

    // Combining fill, alignment, sign, width, precision, and type
    std::cout << "Combined Format Specifiers:\n";
    std::cout << fmt::format("[{:*>+12.2e}]", val1) << " (right, * fill, + sign, precision 2, scientific)\n";
    std::cout << fmt::format("[{:-<+12.2f}]", val2) << " (left, - fill, + sign, precision 2, fixed)\n";

    return 0;
}

#else

int main()
{
    std::cout << "<fmt/format> is unsupported" << std::endl;
    return 0;
}

#endif
