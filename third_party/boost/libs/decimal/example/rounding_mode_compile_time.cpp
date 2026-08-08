// Copyright 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// To define a global compile-time rounding mode
// you must define the macro before inclusion of *ANY* decimal header
#define BOOST_DECIMAL_FE_DEC_DOWNWARD

#include <boost/decimal/decimal32_t.hpp>    // For type decimal32_t
#include <boost/decimal/literals.hpp>       // For decimal literals
#include <boost/decimal/iostream.hpp>       // For decimal <iostream> support
#include <boost/decimal/cfenv.hpp>          // For rounding mode access
#include <iostream>

void print_rounding_mode(const boost::decimal::rounding_mode current_mode)
{
    // All 5 rounding modes are defined by the enum rounding_mode
    using boost::decimal::rounding_mode;

    switch (current_mode)
    {
        case rounding_mode::fe_dec_downward:
            std::cout << "fe_dec_downward\n";
            break;
        case rounding_mode::fe_dec_to_nearest:
            std::cout << "fe_dec_to_nearest\n";
            break;
        case rounding_mode::fe_dec_to_nearest_from_zero:
            std::cout << "fe_dec_to_nearest_from_zero\n";
            break;
        case rounding_mode::fe_dec_toward_zero:
            std::cout << "fe_dec_toward_zero\n";
            break;
        case rounding_mode::fe_dec_upward:
            std::cout << "fe_dec_upward\n";
            break;
    }
}

int main()
{
    using namespace boost::decimal::literals;
    using boost::decimal::decimal32_t;

    // This uses one of the same examples from our runtime rounding mode example
    // Now we can see the effects on the generation of constants,
    // since we can static_assert the result

    constexpr decimal32_t lhs {"5e+50"_DF};
    constexpr decimal32_t rhs {"4e+40"_DF};
    constexpr decimal32_t downward_res {lhs - rhs};
    static_assert(downward_res == "4.999999e+50"_DF, "Incorrectly rounded result");

    std::cout << "The default rounding mode is: ";
    print_rounding_mode(boost::decimal::rounding_mode::fe_dec_default);

    // Here we can see that the rounding mode has been set to something besides default
    // without having had to call fesetround
    //
    // This works with all compilers unlike changing the rounding mode at run-time
    std::cout << "The current rounding mode is: ";
    print_rounding_mode(boost::decimal::fegetround());
}
