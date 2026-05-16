// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// See: https://github.com/boostorg/decimal/issues/1055

#ifndef BOOST_DECIMAL_USE_MODULE
#include <boost/decimal.hpp>
#endif

#include <boost/core/lightweight_test.hpp>
#include <iostream>

#ifdef BOOST_DECIMAL_USE_MODULE
import boost.decimal;
#endif

using namespace boost::decimal;

template <typename DecimalType>
void endpos_using_istream(const std::string& str, const int expected_endpos)
{
    std::istringstream is(str);
    DecimalType x;

    is >> x;
    is.clear();
    const auto endpos = is.tellg();

    if (!BOOST_TEST_EQ(endpos, expected_endpos))
    {
        // LCOV_EXCL_START
        std::cerr << "String: " << str << '\n'
                  << "Expected: " << expected_endpos << '\n'
                  << "Got: " << is.tellg() << std::endl;
        // LCOV_EXCL_STOP
    }
}

template <typename DecimalType>
void check_endpos()
{
    // Expected end positions match double handling with GCC 15.2 on x64
    endpos_using_istream<DecimalType>("Decimal!", 0);
    endpos_using_istream<DecimalType>("127.0.0.1", 5);
    endpos_using_istream<DecimalType>("nullptr", 0);

    // Handling of nan and infinity is already an exception to what is supposed to happen
    // We'll claim that it's like the IP address case where everything past INF or NAN is considered junk
    endpos_using_istream<DecimalType>("INF", 3);
    endpos_using_istream<DecimalType>("INFinity", 3);
    endpos_using_istream<DecimalType>("INFinite", 3);

    endpos_using_istream<DecimalType>("nan", 3);
    endpos_using_istream<DecimalType>("nanfinity", 3);
    endpos_using_istream<DecimalType>("nan(snan)", 9);
    endpos_using_istream<DecimalType>("nan(snan)JUNK", 9);
}

int main()
{
    check_endpos<decimal32_t>();
    check_endpos<decimal64_t>();
    check_endpos<decimal128_t>();

    check_endpos<decimal_fast32_t>();
    check_endpos<decimal_fast64_t>();
    check_endpos<decimal_fast128_t>();

    return boost::report_errors();
}
