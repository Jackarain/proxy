// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_USE_MODULE
#include <boost/decimal.hpp>
#endif

#include <boost/core/lightweight_test.hpp>
#include <iostream>

#ifdef BOOST_DECIMAL_USE_MODULE
import boost.decimal;
#endif

#if BOOST_DECIMAL_ENDIAN_LITTLE_BYTE

int main()
{
    constexpr std::uint64_t reference_bits {0xB1800000000002EE};
    constexpr boost::decimal::decimal64_t test_value {-750, -2};

    std::uint64_t test_value_bits;
    std::memcpy(&test_value_bits, &test_value, sizeof(test_value));

    BOOST_TEST_EQ(test_value_bits, reference_bits);

    return boost::report_errors();
}

#else

int main()
{
    return 0;
}

#endif
