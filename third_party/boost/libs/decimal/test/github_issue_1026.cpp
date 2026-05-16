// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// See: https://github.com/boostorg/decimal/issues/1026

#ifndef BOOST_DECIMAL_USE_MODULE
#include <boost/decimal.hpp>
#endif

#include <boost/core/lightweight_test.hpp>
#include <iostream>

#ifdef BOOST_DECIMAL_USE_MODULE
import boost.decimal;
#endif

using namespace boost::decimal;
using namespace boost::decimal::literals;

int main()
{
    // Round ties to even

    BOOST_TEST_EQ("1234567.49"_DF, "1234567"_DF);
    BOOST_TEST_EQ("1234567.50"_DF, "1234568"_DF);
    BOOST_TEST_EQ("1234567.51"_DF, "1234568"_DF);

    BOOST_TEST_EQ("9999999991234567.49"_DD, "9999999991234567"_DD);
    BOOST_TEST_EQ("9999999991234567.50"_DD, "9999999991234568"_DD);
    BOOST_TEST_EQ("9999999991234567.51"_DD, "9999999991234568"_DD);

    BOOST_TEST_EQ("9999999999999999999999999991234567.49"_DL, "9999999999999999999999999991234567"_DL);
    BOOST_TEST_EQ("9999999999999999999999999991234567.50"_DL, "9999999999999999999999999991234568"_DL);
    BOOST_TEST_EQ("9999999999999999999999999991234567.51"_DL, "9999999999999999999999999991234568"_DL);

    BOOST_TEST_EQ("2345678.49"_DF, "2345678"_DF);
    BOOST_TEST_EQ("2345678.50"_DF, "2345678"_DF);
    BOOST_TEST_EQ("2345678.51"_DF, "2345679"_DF);

    BOOST_TEST_EQ("9999999992345678.49"_DD, "9999999992345678"_DD);
    BOOST_TEST_EQ("9999999992345678.50"_DD, "9999999992345678"_DD);
    BOOST_TEST_EQ("9999999992345678.51"_DD, "9999999992345679"_DD);

    BOOST_TEST_EQ("9999999999999999999999999992345678.49"_DL, "9999999999999999999999999992345678"_DL);
    BOOST_TEST_EQ("9999999999999999999999999992345678.50"_DL, "9999999999999999999999999992345678"_DL);
    BOOST_TEST_EQ("9999999999999999999999999992345678.51"_DL, "9999999999999999999999999992345679"_DL);
    BOOST_TEST_EQ("145433.2908011933696719165119928295655062562131932287426051970822"_DL, "145433.2908011933696719165119928296"_DL);
    BOOST_TEST_EQ("30269.587755640502150977251770554"_DL * "4.8046009735990873395936309640543"_DL, "145433.2908011933696719165119928296"_DL);

    BOOST_TEST_EQ(("0"_DF + "8.4e-96"_DF), "8.4e-96"_DF);
    BOOST_TEST_EQ(("0"_DF + std::numeric_limits<decimal32_t>::denorm_min()), std::numeric_limits<decimal32_t>::denorm_min());
    BOOST_TEST_EQ((std::numeric_limits<decimal32_t>::denorm_min() + std::numeric_limits<decimal32_t>::denorm_min()), 2*std::numeric_limits<decimal32_t>::denorm_min());

    BOOST_TEST_EQ(("0"_DD + "8.4e-96"_DD), "8.4e-96"_DD);
    BOOST_TEST_EQ(("0"_DD + std::numeric_limits<decimal64_t>::denorm_min()), std::numeric_limits<decimal64_t>::denorm_min());
    BOOST_TEST_EQ((std::numeric_limits<decimal64_t>::denorm_min() + std::numeric_limits<decimal64_t>::denorm_min()), 2*std::numeric_limits<decimal64_t>::denorm_min());

    BOOST_TEST_EQ("0"_DF + "8.4e-100"_DF, "8.4e-100"_DF);
    BOOST_TEST_EQ("1"_DF * "1e-101"_DF, "1e-101"_DF);
    BOOST_TEST_EQ("1e-101"_DF / "1"_DF, "1e-101"_DF);

    BOOST_TEST_EQ("0"_DD + "8.4e-100"_DD, "8.4e-100"_DD);
    BOOST_TEST_EQ("1"_DD * "1e-101"_DD, "1e-101"_DD);
    BOOST_TEST_EQ("1e-101"_DD / "1"_DD, "1e-101"_DD);

    BOOST_TEST_EQ("5.24289e-96"_DF / "1"_DF, "5.24289e-96"_DF);

    BOOST_TEST_EQ("1"_DF / "5.24289e-96"_DF, "1.907345e+95"_DF);

    #ifndef BOOST_DECIMAL_NO_CONSTEVAL_DETECTION

    const auto new_rounding_mode = fesetround(rounding_mode::fe_dec_to_nearest_from_zero);
    BOOST_TEST(new_rounding_mode == rounding_mode::fe_dec_to_nearest_from_zero);
    BOOST_TEST_EQ(decimal32_t(100000, -105), "1e-100"_df);

    #endif // BOOST_DECIMAL_NO_CONSTEVAL_DETECTION

    return boost::report_errors();
}

