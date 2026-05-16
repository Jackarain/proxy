// Copyright 2024 - 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// This example demonstrates how to set and get the global rounding mode
// as well as the effects on numerical results

#include <boost/decimal/decimal32_t.hpp>    // For type decimal32_t
#include <boost/decimal/literals.hpp>       // For decimal literals
#include <boost/decimal/cfenv.hpp>          // For access to the rounding mode functions
#include <boost/decimal/iostream.hpp>       // Decimal support to <iostream>
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
    // The rounding mode can only be changed at run-time if the compiler supports
    // 1. C++20 std::is_constant_evaluated()
    // 2. Intrinsics that do the same
    // If neither of the above are defined the library defines BOOST_DECIMAL_NO_CONSTEVAL_DETECTION

    // The current rounding mode can be queried with boost::decimal::fegetround
    const boost::decimal::rounding_mode default_rounding_mode = boost::decimal::fegetround();
    std::cout << "The default rounding mode is: ";
    print_rounding_mode(default_rounding_mode);

    // To set a new rounding mode use boost::decimal::fesetround
    // fesetround returns current mode after updating the global state
    //
    // If your compiler set defines BOOST_DECIMAL_NO_CONSTEVAL_DETECTION the global state can not be updated,
    // so this can be a useful check to make sure that state is what you expect it to be
    auto new_rounding_mode = boost::decimal::fesetround(boost::decimal::rounding_mode::fe_dec_upward);
    std::cout << "The current rounding mode is: ";
    print_rounding_mode(new_rounding_mode);

    #ifndef BOOST_DECIMAL_NO_CONSTEVAL_DETECTION

    using namespace boost::decimal::literals;
    using boost::decimal::decimal32_t;

    const decimal32_t lhs {"5e+50"_DF};
    const decimal32_t rhs {"4e+40"_DF};

    std::cout << "lhs equals: " << lhs << '\n'
              << "rhs equals: " << rhs << '\n';

    // With upward rounding the result will be "5.000001e+50"_DF
    // Even though the difference in order of magnitude is greater than the precision of the type,
    // any addition in this mode will result in at least a one ULP difference
    const decimal32_t upward_res {lhs + rhs};
    std::cout << "  Sum with upward rounding: " << upward_res << '\n';


    new_rounding_mode = boost::decimal::fesetround(boost::decimal::rounding_mode::fe_dec_downward);
    std::cout << "The current rounding mode is: ";
    print_rounding_mode(new_rounding_mode);

    // Similar to above in the downward rounding mode any subtraction will result in at least a one ULP difference
    const decimal32_t downward_res {lhs - rhs};
    std::cout << "Sum with downward rounding: " << downward_res << '\n';

    #endif // BOOST_DECIMAL_NO_CONSTEVAL_DETECTION
}
