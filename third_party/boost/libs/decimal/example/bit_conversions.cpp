// Copyright 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/decimal/decimal32_t.hpp>
#include <boost/decimal/dpd_conversion.hpp>
#include <boost/decimal/bid_conversion.hpp>
#include <iostream>

int main()
{
    using boost::decimal::decimal32_t;
    using boost::decimal::from_bid;
    using boost::decimal::from_dpd;
    using boost::decimal::to_bid;
    using boost::decimal::to_dpd;

    // First we construct a decimal value and then convert it into both BID and DPD encoded bits
    const decimal32_t decimal_value {5};
    const std::uint32_t BID_bits {to_bid(decimal_value)};
    const std::uint32_t DPD_bits {to_dpd(decimal_value)};

    // Display the difference between the hex values of both bit encodings
    std::cout << std::hex
              << "BID format: " << BID_bits << '\n'
              << "DPD format: " << DPD_bits << std::endl;

    // Recover the original value by encoding two new decimals using the from_bid and from_dpd functions
    const decimal32_t bid_decimal {from_bid<decimal32_t>(BID_bits)};
    const decimal32_t dpd_decimal {from_dpd<decimal32_t>(DPD_bits)};

    if (bid_decimal == dpd_decimal)
    {
        // These should both have recovered the original value of 5
        return 0;
    }
    else
    {
        // Something has gone wrong recovering the decimal values
        return 1;
    }
}
