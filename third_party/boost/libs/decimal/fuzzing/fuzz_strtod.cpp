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
        std::string c_data_str {c_data, size}; // Guarantee null termination since we can't pass the size argument

        char** endptr = nullptr;

        boost::decimal::strtod32(c_data_str.c_str(), endptr);

        boost::decimal::strtod64(c_data_str.c_str(), endptr);

        boost::decimal::strtod128(c_data_str.c_str(), endptr);
    }
    catch(...)
    {
        std::cerr << "Error with: " << data << std::endl;
        std::terminate();
    }

    return 0;
}
