// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include "mini_to_chars.hpp"
#include <cfloat>
#include <boost/decimal/decimal32_t.hpp>
#include <boost/decimal/decimal64_t.hpp>
#include <boost/decimal/decimal128_t.hpp>
#include <boost/decimal/decimal_fast32_t.hpp>
#include <boost/decimal/decimal_fast64_t.hpp>
#include <boost/decimal/decimal_fast128_t.hpp>
#include <boost/decimal/string.hpp>
#include <boost/decimal/iostream.hpp>
#include <boost/decimal/literals.hpp>
#include <boost/core/lightweight_test.hpp>

using namespace boost::decimal;
using namespace boost::decimal::literals;

void test_decimal32_t_literals()
{
    BOOST_TEST_EQ(decimal32_t(0), 0_DF);
    BOOST_TEST_EQ(decimal32_t(3), 3_DF);
    BOOST_TEST_EQ(decimal32_t(3.1), 3.1_DF);
    BOOST_TEST_EQ(decimal32_t(3, 1), 3e1_DF);
    BOOST_TEST(isinf(5e100_DF));
    BOOST_TEST(isinf(5e300_DF));

    BOOST_TEST_EQ(decimal32_t(0), 0_df);
    BOOST_TEST_EQ(decimal32_t(3), 3_df);
    BOOST_TEST_EQ(decimal32_t(3.1), 3.1_df);
    BOOST_TEST_EQ(decimal32_t(3, 1), 3e1_df);
    BOOST_TEST(isinf(5e100_df));
    BOOST_TEST(isinf(5e300_df));
}

void test_decimal_fast32_t_literals()
{
    BOOST_TEST_EQ(decimal_fast32_t(0), 0_DFF);
    BOOST_TEST_EQ(decimal_fast32_t(3), 3_DFF);
    BOOST_TEST_EQ(decimal_fast32_t(3.1), 3.1_DFF);
    BOOST_TEST_EQ(decimal_fast32_t(3, 1), 3e1_DFF);
    BOOST_TEST(isinf(5e100_DFF));
    BOOST_TEST(isinf(5e300_DFF));

    BOOST_TEST_EQ(decimal_fast32_t(0), 0_dff);
    BOOST_TEST_EQ(decimal_fast32_t(3), 3_dff);
    BOOST_TEST_EQ(decimal_fast32_t(3.1), 3.1_dff);
    BOOST_TEST_EQ(decimal_fast32_t(3, 1), 3e1_dff);
    BOOST_TEST(isinf(5e100_dff));
    BOOST_TEST(isinf(5e300_dff));
}

void test_decimal64_t_literals()
{
    BOOST_TEST_EQ(decimal64_t(0), 0_DD);
    BOOST_TEST_EQ(decimal64_t(3), 3_DD);
    BOOST_TEST_EQ(decimal64_t(3.1), 3.1_DD);
    BOOST_TEST_EQ(decimal64_t(3, 1), 3e1_DD);

    BOOST_TEST_EQ(decimal64_t(0), 0_dd);
    BOOST_TEST_EQ(decimal64_t(3), 3_dd);
    BOOST_TEST_EQ(decimal64_t(3.1), 3.1_dd);
    BOOST_TEST_EQ(decimal64_t(3, 1), 3e1_dd);

    // 64-bit long double warn of overflow
    #if !(LDBL_MANT_DIG == 53 && LDBL_MAX_EXP == 1024)
    BOOST_TEST(isinf(5e1000_dd));
    BOOST_TEST(isinf(5e3000_dd));
    BOOST_TEST(isinf(5e1000_DD));
    BOOST_TEST(isinf(5e3000_DD));
    #endif
}

void test_decimal_fast64_t_literals()
{
    BOOST_TEST_EQ(decimal_fast64_t(0), 0_DDF);
    BOOST_TEST_EQ(decimal_fast64_t(3), 3_DDF);
    BOOST_TEST_EQ(decimal_fast64_t(3.1), 3.1_DDF);
    BOOST_TEST_EQ(decimal_fast64_t(3, 1), 3e1_DDF);

    BOOST_TEST_EQ(decimal_fast64_t(0), 0_ddf);
    BOOST_TEST_EQ(decimal_fast64_t(3), 3_ddf);
    BOOST_TEST_EQ(decimal_fast64_t(3.1), 3.1_ddf);
    BOOST_TEST_EQ(decimal_fast64_t(3, 1), 3e1_ddf);

    // 64-bit long double warn of overflow
    #if !(LDBL_MANT_DIG == 53 && LDBL_MAX_EXP == 1024)
    BOOST_TEST(isinf(5e1000_ddf));
    BOOST_TEST(isinf(5e3000_ddf));
    BOOST_TEST(isinf(5e1000_DDF));
    BOOST_TEST(isinf(5e3000_DDF));
    #endif
}

void test_decimal128_t_literals()
{
    BOOST_TEST_EQ(decimal128_t(0), 0_DL);
    BOOST_TEST_EQ(decimal128_t(3), 3_DL);
    BOOST_TEST_EQ(decimal128_t(3.1), 3.1_DL);
    BOOST_TEST_EQ(decimal128_t(3, 1), 3e1_DL);

    BOOST_TEST_EQ(decimal128_t(0), 0_dl);
    BOOST_TEST_EQ(decimal128_t(3), 3_dl);
    BOOST_TEST_EQ(decimal128_t(3.1), 3.1_dl);
    BOOST_TEST_EQ(decimal128_t(3, 1), 3e1_dl);
}

void test_decimal_fast128_t_literals()
{
    BOOST_TEST_EQ(decimal_fast128_t(0), 0_DLF);
    BOOST_TEST_EQ(decimal_fast128_t(3), 3_DLF);
    BOOST_TEST_EQ(decimal_fast128_t(3.1), 3.1_DLF);
    BOOST_TEST_EQ(decimal_fast128_t(3, 1), 3e1_DLF);

    BOOST_TEST_EQ(decimal_fast128_t(0), 0_dlf);
    BOOST_TEST_EQ(decimal_fast128_t(3), 3_dlf);
    BOOST_TEST_EQ(decimal_fast128_t(3.1), 3.1_dlf);
    BOOST_TEST_EQ(decimal_fast128_t(3, 1), 3e1_dlf);
}

// https://github.com/boostorg/decimal/issues/1058
void construct_negative_infinity()
{
    using namespace boost::decimal::literals;
    BOOST_TEST_EQ("-inf"_DF, -"inf"_DF);
    BOOST_TEST_EQ("-inf"_DD, -"inf"_DD);
    BOOST_TEST_EQ("-inf"_DL, -"inf"_DL);
    BOOST_TEST_EQ("-inf"_DFF, -"inf"_DFF);
    BOOST_TEST_EQ("-inf"_DDF, -"inf"_DDF);
    BOOST_TEST_EQ("-inf"_DLF, -"inf"_DLF);
}

void test_issue_1119()
{
    using namespace boost::decimal::literals;

    const auto val = 1000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000_DD;
    BOOST_TEST_EQ(val, decimal64_t(1, 198));

    const auto overflow_val = 1000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000_df;
    BOOST_TEST(isinf(overflow_val));
    BOOST_TEST(!signbit(overflow_val));
}

int main()
{
    test_decimal32_t_literals();
    test_decimal64_t_literals();
    test_decimal128_t_literals();

    test_decimal_fast32_t_literals();
    test_decimal_fast64_t_literals();
    test_decimal_fast128_t_literals();

    construct_negative_infinity();

    test_issue_1119();

    return boost::report_errors();
}
