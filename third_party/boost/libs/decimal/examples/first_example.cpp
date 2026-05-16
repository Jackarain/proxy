// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/decimal.hpp>    // This includes the entire decimal library
#include <iostream>
#include <iomanip>

int main()
{
    using namespace boost::decimal::literals; // The literals are in their own namespace like std::literals

    // First we show the result of 0.1 + 0.2 using regular doubles
    std::cout << std::fixed << std::setprecision(17)
              << "Using doubles:\n"
              << "0.1 + 0.2 = " << 0.1 + 0.2 << "\n\n";

    // Construct the two decimal values
    // We construct using the literals defined by the library
    constexpr boost::decimal::decimal64_t a {0.1_DD};
    constexpr boost::decimal::decimal64_t b {0.2_DD};

    // Now we display the result of the same calculation using decimal64_t
    std::cout << "Using decimal64_t:\n"
              << "0.1_DD + 0.2_DD = " << a + b << std::endl;
}
