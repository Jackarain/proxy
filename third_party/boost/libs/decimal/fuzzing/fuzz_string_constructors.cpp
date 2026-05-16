// Copyright 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/decimal.hpp>
#include <iostream>
#include <exception>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    try
    {
        auto c_data = reinterpret_cast<const char*>(data);
        const std::string c_data_str {c_data, size}; // Guarantee null termination since we can't pass the size argument

        const boost::decimal::decimal32_t d32_val {c_data_str};
        const boost::decimal::decimal64_t d64_val {c_data_str};
        const boost::decimal::decimal128_t d128_val {c_data_str};

        const boost::decimal::decimal_fast32_t df32_val {c_data_str};
        const boost::decimal::decimal_fast64_t df64_val {c_data_str};
        const boost::decimal::decimal_fast128_t df128_val {c_data_str};

        static_cast<void>(d32_val);
        static_cast<void>(d64_val);
        static_cast<void>(d128_val);

        static_cast<void>(df32_val);
        static_cast<void>(df64_val);
    }
    catch(...)
    {
    }

    return 0;
}
