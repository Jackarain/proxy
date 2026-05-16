// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// This file briefly demonstrates the results of mixed decimal comparisons and arithmetic

#include <boost/decimal/decimal32_t.hpp>    // For the type decimal32_t
#include <boost/decimal/decimal64_t.hpp>    // For the type decimal64_t
#include <boost/decimal/iostream.hpp>       // For decimal type support to <iostream>
#include <type_traits>
#include <iostream>
#include <limits>

int main()
{
    using boost::decimal::decimal32_t;
    using boost::decimal::decimal64_t;
    using boost::decimal::decimal128_t;

    // First construct two values that we will perform arithmetic with
    const decimal32_t a {"5.2"};
    const decimal64_t b {"3.9"};

    std::cout << "decimal32_t value (a): " << a << '\n'
              << "decimal64_t value (b): " << b << '\n';

    // Mixed decimal comparisons are allowed by default
    if (a > b)
    {
        std::cout << "a is greater than b" << '\n';
    }

    // Even comparison of unrepresentable values is fine
    // For example decimal32_t can't represent decimal64_t max value
    constexpr decimal64_t dec64_max {std::numeric_limits<decimal64_t>::max()};
    if (a < dec64_max)
    {
        std::cout << a << " is less than " << dec64_max << '\n';
    }

    // Danger awaits if you decide to do this yourself instead of letting the system do it for you,
    // since in this example the two should compare equal but overflowing decimal32_t makes infinity
    if (static_cast<decimal32_t>(dec64_max) < dec64_max)
    {
        std::cout << dec64_max << " is less than " << static_cast<decimal32_t>(dec64_max) << '\n';
    }

    // With mixed operations like +, -, *, / we promote to the higher precision type
    // Example: decimal32_t + decimal64_t -> decimal64_t

    // We use auto here for two reasons
    // 1) To demonstrate that it's safe
    // 2) To show the promotion with the conditional logic that follows
    const auto c {a + b};
    using c_type = std::remove_cv_t<decltype(c)>; // We used const auto so the result is const decimal64_t

    static_assert(std::is_same<c_type, decimal64_t>::value, "decimal32_t + decimal64_t is supposed to yield decimal64_t");
    std::cout << "The result of a + b is a decimal64_t: " << c << '\n';

    // Now we can look at similar promotion that occurs when an operation is performed between
    // a decimal type and an integer
    //
    // Similar to the above when we have mixed operations like +, -, *, / we always promote to the decimal type
    // Example: decimal64_t * int -> decimal64_t

    const auto d {2 * c};
    using d_type = std::remove_cv_t<decltype(d)>;
    static_assert(std::is_same<d_type, decimal64_t>::value, "decimal64_t * integer is supposed to yield decimal64_t");
    std::cout << "The result of 2 * c is a decimal64_t: " << d << '\n';

    // The full suite of comparison operators between decimal types and integers
    if (d > 5)
    {
        std::cout << d << " is greater than 5" << '\n';
    }
}
