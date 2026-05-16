// Copyright 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/decimal.hpp>
#include <iostream>

int main()
{
    constexpr boost::decimal::decimal32_t d {1, 2};

    boost::decimal::printf("%Hg", d);
    std::cout << std::endl;

    boost::decimal::printf("%Hg %Hg", d, d);
    std::cout << std::endl;

    boost::decimal::printf("%Hg %Hg %Hg", d, d, d);
    std::cout << std::endl;

    boost::decimal::printf("Value 1: %Hg, Value 2: %Hg, Value 3: %Hg", d, d, d);
    std::cout << std::endl;

    return 1;
}
