// Copyright 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// This examples demonstrates decimal floating point literals,
// as well as numeric constants made available by the library

#include <boost/decimal/decimal32_t.hpp>        // For type decimal32_t
#include <boost/decimal/decimal64_t.hpp>        // For type decimal64_t
#include <boost/decimal/literals.hpp>           // For the decimal (user defined) literals
#include <boost/decimal/numbers.hpp>            // For provided numeric constants
#include <boost/decimal/iostream.hpp>           // For support to <iostream> and <iomanip>
#include <iostream>
#include <iomanip>
#include <type_traits>
#include <limits>

int main()
{
    using namespace boost::decimal::literals;   // Much like the std namespace, literals are separate form the lib
    using boost::decimal::decimal32_t;  // Type decimal32_t
    using boost::decimal::decimal64_t;  // Type decimal64_t

    // Defaulted numeric constants are available with type decimal64_t,
    // much like std::numbers::pi defaults to double
    constexpr auto default_pi {boost::decimal::numbers::pi};
    using default_type = std::remove_cv_t<decltype(default_pi)>;
    static_assert(std::is_same<default_type, decimal64_t>::value, "Defaulted value has type decimal64_t");

    // You can also specify the type explicitly as these are template constants
    constexpr decimal32_t decimal32_pi {boost::decimal::numbers::pi_v<decimal32_t>};

    // We can use std::setprecision from <iomanip> to see the real difference between the two values
    // numeric_limits is also specialized for each type
    std::cout << std::setprecision(std::numeric_limits<decimal32_t>::digits10)
              << "32-bit Pi: " << decimal32_pi << '\n';

    std::cout << std::setprecision(std::numeric_limits<decimal64_t>::digits10)
              << "64-bit Pi: " << default_pi << '\n';

    // All of our types also offer user defined literals:
    // _df or _DF for decimal32_t
    // _dd or _DD for decimal64_t
    // _dl or _DL for decimal128_t
    // For fast types add an f to the end of each (e.g. _dff = decimal_fast32_t or _DLF decimal_fast128_t)
    //
    // Since we have specified the type using the literal it is safe to use auto for the type
    //
    // We construct both from the first 40 digits of pi
    // The constructor will parse this and then round to the proper precision automatically

    constexpr auto literal32_pi {"3.141592653589793238462643383279502884197"_DF};
    constexpr auto literal64_pi {"3.141592653589793238462643383279502884197"_DD};

    std::cout << std::setprecision(std::numeric_limits<decimal32_t>::digits10)
              << "32-bit UDL Pi: " << literal32_pi << '\n';

    // Unlike built-in binary floating point, floating equal is acceptable with decimal floating point
    // Float equal will automatically address cohorts as per IEEE 754 if required (not shown in this example)
    if (literal32_pi == decimal32_pi)
    {
        std::cout << "Rounded UDL has the same value as the 32-bit constant" << '\n';
    }

    std::cout << std::setprecision(std::numeric_limits<decimal64_t>::digits10)
              << "64-bit UDL Pi: " << literal64_pi << '\n';

    if (literal64_pi == default_pi)
    {
        std::cout << "Rounded UDL has the same value as the 64-bit constant" << '\n';
    }
}
