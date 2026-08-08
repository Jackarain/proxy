// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// This file demonstrates the effects of cohorts and how to maintain them with <charconv>

#include <boost/decimal/decimal32_t.hpp>    // For the type decimal32_t
#include <boost/decimal/charconv.hpp>       // For decimal support for <charconv>
#include <boost/decimal/iostream.hpp>       // For decimal support for <iostream>
#include <iostream>
#include <array>
#include <cstring>
#include <string>

static constexpr std::size_t N {7};

// All the following decimal values will compare equal,
// but since they have different numbers of 0s in the significand they will not be bitwise equal
constexpr std::array<boost::decimal::decimal32_t, N> decimals = {
    boost::decimal::decimal32_t{3, 2},
    boost::decimal::decimal32_t{30, 1},
    boost::decimal::decimal32_t{300, 0},
    boost::decimal::decimal32_t{3000, -1},
    boost::decimal::decimal32_t{30000, -2},
    boost::decimal::decimal32_t{300000, -3},
    boost::decimal::decimal32_t{3000000, -4},
};

// These strings represent the same values as the constructed ones shown above
constexpr std::array<const char*, N> strings = {
    "3e+02",
    "3.0e+02",
    "3.00e+02",
    "3.000e+02",
    "3.0000e+02",
    "3.00000e+02",
    "3.000000e+02",
};

int main()
{
    using boost::decimal::decimal32_t;      // For type decimal32_t
    using boost::decimal::from_chars;       // decimal specific from_chars
    using boost::decimal::chars_format;     // chars_format enum with decimal specific option shown here

    // In some instances we want to preserve the cohort of our values
    // In the above strings array all of these values compare equal,
    // but will NOT be bitwise equal once constructed.

    for (std::size_t i = 0; i < N; ++i)
    {
        decimal32_t string_val;
        const auto r_from = from_chars(strings[i], string_val, chars_format::cohort_preserving_scientific);

        if (!r_from)
        {
            // Unexpected failure
            return 1;
        }

        for (std::size_t j = 0; j < N; ++j)
        {
            // Now that we have constructed a value from string
            // we can compare it bitwise to all the members of the decimal array
            // to show the difference between operator== and bitwise equality
            //
            // All members of a cohort are supposed to compare equal with operator==,
            // and likewise will hash equal to
            std::uint32_t string_val_bits;
            std::uint32_t constructed_val_bits;

            std::memcpy(&string_val_bits, &string_val, sizeof(string_val_bits));
            std::memcpy(&constructed_val_bits, &decimals[j], sizeof(constructed_val_bits));

            if (string_val == decimals[j])
            {
                std::cout << "Values are equal and ";
                if (string_val_bits == constructed_val_bits)
                {
                    std::cout << "bitwise equal.\n";
                }
                else
                {
                    std::cout << "NOT bitwise equal.\n";
                }
            }
        }

        // The same chars_format option applies to to_chars which allows us to roundtrip the values
        char buffer[boost::decimal::formatting_limits<decimal32_t>::cohort_preserving_scientific_max_chars] {};
        const auto r_to = to_chars(buffer, buffer + sizeof(buffer), string_val, chars_format::cohort_preserving_scientific);

        if (!r_to)
        {
            // Unexpected failure
            return 1;
        }

        *r_to.ptr = '\0'; // charconv does not null terminate per the C++ specification

        if (std::strcmp(strings[i], buffer) == 0)
        {
            std::cout << "Successful Roundtrip of value: " << buffer << "\n\n";
        }
        else
        {
            std::cout << "Failed\n\n";
            return 1;
        }
    }

    return 0;
}
