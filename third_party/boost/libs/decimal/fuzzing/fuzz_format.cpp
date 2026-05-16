// Copyright 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/decimal.hpp>

#ifdef BOOST_DECIMAL_HAS_FORMAT_SUPPORT

#include <iostream>
#include <exception>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{ 
    try
    {
        auto c_data = reinterpret_cast<const char*>(data);

        const auto formats = {boost::decimal::chars_format::general, boost::decimal::chars_format::fixed,
                              boost::decimal::chars_format::scientific, boost::decimal::chars_format::hex};

        for (const auto format : formats)
        {
            boost::decimal::decimal32_t f_val {};
            boost::decimal::from_chars(c_data, c_data + size, f_val, format);

            std::format("{}", f_val);

            boost::decimal::decimal64_t val {};
            boost::decimal::from_chars(c_data, c_data + size, val, format);

            std::format("{}", val);

            boost::decimal::decimal128_t ld_val {};
            boost::decimal::from_chars(c_data, c_data + size, ld_val, format);

            std::format("{}", ld_val);
        }
    }
    catch (const std::format_error&)
    {
        // It is expected that many paths will have invalid format strings
        return 0;
    }
    catch (...)
    {
        std::cerr << "Error with: " << data << std::endl;
        std::terminate();
    }

    return 0;
}

#else

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t*, std::size_t)
{
    return 0;
}

#endif
