// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// See: https://github.com/boostorg/decimal/issues/1035

#ifndef BOOST_DECIMAL_USE_MODULE
#include <boost/decimal.hpp>
#endif

#include <boost/core/lightweight_test.hpp>
#include <iostream>
#include <random>

#ifdef BOOST_DECIMAL_USE_MODULE
import boost.decimal;
#endif

using namespace boost::decimal;
using namespace boost::decimal::literals;

int main()
{
    std::mt19937_64 rng(42);
    std::uniform_int_distribution<int> dist(1, 1);

    const auto previously_inf {"5e+95"_DF * dist(rng)};
    BOOST_TEST_EQ(previously_inf, "500000e+90"_DF);

    #ifdef BOOST_DECIMAL_NO_CONSTEVAL_DETECTION
    return boost::report_errors();
    #else

    fesetround(rounding_mode::fe_dec_downward);
    BOOST_TEST_EQ("5e+50"_DF * dist(rng) - "4e+40"_DF, "4.999999e+50"_DF);
    BOOST_TEST_EQ("5e+95"_DF * dist(rng)  - "4e-100"_DF, "4.999999e+95"_DF);
    BOOST_TEST_EQ("-5e+95"_DF * dist(rng)  + "4e-100"_DF, "-4.999999e+95"_DF);
    BOOST_TEST_EQ("-5e+95"_DL * dist(rng)  + "4e-100"_DL, "-4.999999999999999999999999999999999e+95"_DL);

    fesetround(rounding_mode::fe_dec_upward);
    BOOST_TEST_EQ("5e+50"_DF * dist(rng) + "4e+40"_DF, "5.000001e+50"_DF);
    BOOST_TEST_EQ("5e+95"_DF * dist(rng)  + "4e-100"_DF, "5.000001e+95"_DF);
    BOOST_TEST_EQ("-5e+95"_DF * dist(rng)  - "4e-100"_DF, "-5.000001e+95"_DF);
    BOOST_TEST_EQ("-5e+95"_DL * dist(rng)  - "4e-100"_DL, "-5.000000000000000000000000000000001e+95"_DL);

    return boost::report_errors();

    #endif
}
