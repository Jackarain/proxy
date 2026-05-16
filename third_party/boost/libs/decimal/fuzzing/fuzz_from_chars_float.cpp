// Copyright 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/decimal.hpp>
#include <iostream>
#include <exception>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{ 
    try
    {
        auto c_data = reinterpret_cast<const char*>(data);

        const auto formats = {boost::decimal::chars_format::general, boost::decimal::chars_format::fixed,
                              boost::decimal::chars_format::scientific, boost::decimal::chars_format::hex};

        for (const auto format : formats)
        {
            boost::decimal::decimal32_t f_val;
            boost::decimal::from_chars(c_data, c_data + size, f_val, format);

            boost::decimal::decimal64_t val;
            boost::decimal::from_chars(c_data, c_data + size, val, format);

            boost::decimal::decimal128_t ld_val;
            boost::decimal::from_chars(c_data, c_data + size, ld_val, format);
        }

        // Now with strings
        const auto c_data_str = std::string(c_data, size);
        for (const auto format : formats)
        {
            boost::decimal::decimal32_t f_val;
            boost::decimal::from_chars(c_data_str, f_val, format);

            boost::decimal::decimal64_t val;
            boost::decimal::from_chars(c_data_str, val, format);

            boost::decimal::decimal128_t ld_val;
            boost::decimal::from_chars(c_data_str, ld_val, format);
        }

        // And string views as applicable
        #ifdef BOOST_DECIMAL_HAS_STD_STRING_VIEW
        const auto c_data_str_view = std::string_view(c_data_str);
        for (const auto format : formats)
        {
            boost::decimal::decimal32_t f_val;
            boost::decimal::from_chars(c_data_str_view, f_val, format);

            boost::decimal::decimal64_t val;
            boost::decimal::from_chars(c_data_str_view, val, format);

            boost::decimal::decimal128_t ld_val;
            boost::decimal::from_chars(c_data_str_view, ld_val, format);
        }
        #endif
    }
    catch(...)
    {
        std::cerr << "Error with: " << data << std::endl;
        std::terminate();
    }

    return 0;
}
