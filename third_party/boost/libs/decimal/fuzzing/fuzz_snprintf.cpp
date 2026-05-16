// Copyright 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/decimal.hpp>
#include <iostream>
#include <exception>
#include <string>
#include <array>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    try
    {
        auto c_data = reinterpret_cast<const char*>(data);

        const auto formats = std::array<boost::decimal::chars_format, 4>{boost::decimal::chars_format::general,
                                                                         boost::decimal::chars_format::fixed,
                                                                         boost::decimal::chars_format::scientific,
                                                                         boost::decimal::chars_format::hex};

        const auto dec32_printf_formats = std::array<const char*, 4>{"%Hg", "%Hf", "%He", "%Ha"};
        const auto dec64_printf_formats = std::array<const char*, 4>{"%Dg", "%Df", "%De", "%Da"};
        const auto dec128_printf_formats = std::array<const char*, 4>{"%DDg", "%DDf", "%DDe", "%DDa"};

        for (std::size_t i {}; i < 4; ++i)
        {
            char buffer[20]; // Small enough it should overflow sometimes

            boost::decimal::decimal32_t f_val {};
            boost::decimal::from_chars(c_data, c_data + size, f_val, formats[i]);
            boost::decimal::snprintf(buffer, sizeof(buffer), dec32_printf_formats[i], f_val);

            boost::decimal::decimal64_t val {};
            boost::decimal::from_chars(c_data, c_data + size, val, formats[i]);
            boost::decimal::snprintf(buffer, sizeof(buffer), dec64_printf_formats[i], val);

            boost::decimal::decimal128_t ld_val {};
            boost::decimal::from_chars(c_data, c_data + size, ld_val, formats[i]);
            boost::decimal::snprintf(buffer, sizeof(buffer), dec128_printf_formats[i], ld_val);
        }
    }
    catch(...)
    {
        std::cerr << "Error with: " << data << std::endl;
        std::terminate();
    }

    return 0;
}
