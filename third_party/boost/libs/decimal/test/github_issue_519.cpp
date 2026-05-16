// Copyright 2024 Matt Borland
// Copyright 2024 Christopher Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/decimal.hpp>

#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>

auto main() -> int
{
    using local_decimal_type = boost::decimal::decimal128_t;

    // N[Gamma[456/100], 34]
    // 12.64819265438397922113369900828315
    //
    // For Decimal 128:
    // 4.56
    // 12.64819265438397922113369900828314

    const local_decimal_type x = local_decimal_type { 456, -2 };

    const auto tg = tgamma(x);

    {
        std::stringstream strm;

        strm << std::setprecision(std::numeric_limits<local_decimal_type>::digits10) << x;

        std::cout << "Expected: 4.56" << std::endl;
        std::cout << "     Got: " << strm.str() << std::endl;
    }

    {
        std::stringstream strm;

        strm << std::setprecision(std::numeric_limits<local_decimal_type>::digits10) << tg;

        std::cout << "Expected: 12.64819265438397922113369900828314" << std::endl;
        std::cout << "     Got: " << strm.str() << std::endl;
    }

    return 1;
}
