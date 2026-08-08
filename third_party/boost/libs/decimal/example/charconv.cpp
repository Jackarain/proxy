// Copyright 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// This file demonstrates the various ways that <charconv> support can be used with the decimal library
// NOTE: <charconv> need not be included to use this functionality

#include <boost/decimal/decimal64_t.hpp>    // For the type decimal64_t
#include <boost/decimal/charconv.hpp>       // For <charconv> support
#include <boost/decimal/iostream.hpp>       // For decimal support <iostream>
#include <iostream>                         // <iostream>
#include <cstring>                          // For std::strlen

int main()
{
    using boost::decimal::decimal64_t;      // The type decimal64_t
    using boost::decimal::to_chars;         // The to_chars functions
    using boost::decimal::to_chars_result;  // The return type of to_chars
    using boost::decimal::from_chars;       // The from_chars functions
    using boost::decimal::from_chars_result;// The return type of from_chars
    using boost::decimal::chars_format;     // The enum class of the different formatting options
    using boost::decimal::formatting_limits;// Allows the user to correctly size buffers

    const char* initial_value {"-7.12345e+06"};

    decimal64_t initial_decimal;
    const from_chars_result r_initial {from_chars(initial_value, initial_value + std::strlen(initial_value), initial_decimal)};

    // from_chars_result contains a value of std::errc, but also has a bool operator for better checks like this
    // Regular <charconv> should be getting this bool operator in C++26
    if (!r_initial)
    {
        // LCOV_EXCL_START
        // Here you can handle an error condition in any way you see fit
        // For the purposes of our example we log and abort
        std::cout << "Unexpected failure" << std::endl;
        return 1;
        // LCOV_EXCL_STOP
    }
    else
    {
        std::cout << "Initial decimal: " << initial_decimal << '\n';
    }

    // boost::decimal::from_chars deviates from the C++ standard by allowing a std::string,
    // or a std::string_view (when available)
    //
    // It is also perfectly acceptable to use an auto return type for even more brevity when using from_chars
    const std::string string_value {"3.1415"};
    decimal64_t string_decimal;
    const auto r_string {from_chars(string_value, string_decimal)};
    if (r_string)
    {
        std::cout << "Value from string: " << string_decimal << '\n';
    }

    // We can now compare the various ways to print the value
    // First we will review the formatting_limits struct
    // This struct contains a number of members that allow the to_chars buffer to be correctly sized
    //
    // First formatting_limits takes a type and optionally a precision as template parameters
    // It then has members each corresponding to the maximum number of characters needed to print the type
    //
    // 1) scientific_format_max_chars
    // 2) fixed_format_max_chars
    // 3) hex_format_max_chars
    // 4) cohort_preserving_scientific_max_chars
    // 5) general_format_max_chars
    // 6) max_chars - Equal to the maximum value of 1 to 5 to allow to_chars of any format
    //
    // Each of these will give you one additional character so you can write a null terminator to the end
    // NOTE: to_chars IS NOT default null terminated

    char scientific_buffer[formatting_limits<decimal64_t>::scientific_format_max_chars];
    const to_chars_result r_sci {to_chars(scientific_buffer,
        scientific_buffer + sizeof(scientific_buffer), initial_decimal, chars_format::scientific)};
    if (r_sci)
    {
        *r_sci.ptr = '\0'; // to_chars does not null terminate per the C++ standard
        std::cout << "Value in scientific format: " << scientific_buffer << '\n';
    }
    // else handle the error how you would like

    // If we went to print the value to some specified precision our buffer will need more space
    // Formatting limits takes a precision in this case
    //
    // Also as with from_chars it's perfectly fine to use an auto return type with to_chars
    constexpr int required_precision {20};
    char precision_20_scientific_buffer[formatting_limits<decimal64_t, required_precision>::scientific_format_max_chars];
    const auto r_sci20 {to_chars(precision_20_scientific_buffer,
        precision_20_scientific_buffer + sizeof(precision_20_scientific_buffer),
        initial_decimal, chars_format::scientific, required_precision)};

    if (r_sci20)
    {
        *r_sci20.ptr = '\0';
        std::cout << "Value in scientific format with precision 20: " << precision_20_scientific_buffer << '\n';
    }
    // else handle the error how you would like
}
