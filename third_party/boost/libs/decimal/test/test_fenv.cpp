// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include "mini_to_chars.hpp"
#include <boost/decimal.hpp>
#include <boost/core/lightweight_test.hpp>

using namespace boost::decimal;

void test_environment()
{
    BOOST_TEST(boost::decimal::fegetround() == boost::decimal::rounding_mode::fe_dec_to_nearest);
    BOOST_TEST(boost::decimal::fesetround(boost::decimal::rounding_mode::fe_dec_to_nearest_from_zero) == boost::decimal::rounding_mode::fe_dec_to_nearest_from_zero);
    BOOST_TEST(boost::decimal::fegetround() == boost::decimal::rounding_mode::fe_dec_to_nearest_from_zero);
    BOOST_TEST(boost::decimal::fesetround(boost::decimal::rounding_mode::fe_dec_downward) == boost::decimal::rounding_mode::fe_dec_downward);
    BOOST_TEST(boost::decimal::fegetround() == boost::decimal::rounding_mode::fe_dec_downward);
    BOOST_TEST(boost::decimal::fesetround(boost::decimal::rounding_mode::fe_dec_upward) == boost::decimal::rounding_mode::fe_dec_upward);
    BOOST_TEST(boost::decimal::fegetround() == boost::decimal::rounding_mode::fe_dec_upward);
}

void test_constructor_rounding()
{
    // Default
    boost::decimal::fesetround(rounding_mode::fe_dec_to_nearest_from_zero);
    BOOST_TEST(boost::decimal::fegetround() == rounding_mode::fe_dec_to_nearest_from_zero);

    BOOST_TEST_EQ(decimal32_t(1, 0), decimal32_t(1, 0));
    BOOST_TEST_EQ(decimal32_t(12'345'675, 0), decimal32_t(1'234'568, 1));
    BOOST_TEST_EQ(decimal32_t(-12'345'675, 0), decimal32_t(-1'234'568, 1));

    // Downward
    boost::decimal::fesetround(rounding_mode::fe_dec_downward);
    BOOST_TEST(boost::decimal::fegetround() == rounding_mode::fe_dec_downward);

    BOOST_TEST_EQ(decimal32_t(1, 0), decimal32_t(1, 0));
    BOOST_TEST_EQ(decimal32_t(12'345'675, 0), decimal32_t(1'234'567, 1));
    BOOST_TEST_EQ(decimal32_t(-12'345'675, 0), decimal32_t(-1'234'568, 1));

    // To nearest (ties go to even)
    boost::decimal::fesetround(rounding_mode::fe_dec_to_nearest);
    BOOST_TEST(boost::decimal::fegetround() == rounding_mode::fe_dec_to_nearest);

    BOOST_TEST_EQ(decimal32_t(1, 0), decimal32_t(1, 0));
    BOOST_TEST_EQ(decimal32_t(12'345'675, 0), decimal32_t(1'234'568, 1));
    BOOST_TEST_EQ(decimal32_t(-12'345'675, 0), decimal32_t(-1'234'568, 1));
    BOOST_TEST_EQ(decimal32_t(55'555'555, 0), decimal32_t(5'555'556, 1));
    BOOST_TEST_EQ(decimal32_t(55'555'556, 0), decimal32_t(5'555'556, 1));

    // Toward zero
    boost::decimal::fesetround(rounding_mode::fe_dec_toward_zero);
    BOOST_TEST(boost::decimal::fegetround() == rounding_mode::fe_dec_toward_zero);

    BOOST_TEST_EQ(decimal32_t(1, 0), decimal32_t(1, 0));
    BOOST_TEST_EQ(decimal32_t(12'345'675, 0), decimal32_t(1'234'567, 1));
    BOOST_TEST_EQ(decimal32_t(-12'345'675, 0), decimal32_t(-1'234'567, 1));

    // Upward
    boost::decimal::fesetround(rounding_mode::fe_dec_upward);
    BOOST_TEST(boost::decimal::fegetround() == rounding_mode::fe_dec_upward);

    BOOST_TEST_EQ(decimal32_t(1, 0), decimal32_t(1, 0));
    BOOST_TEST_EQ(decimal32_t(12'345'675, 0), decimal32_t(1'234'568, 1));
    BOOST_TEST_EQ(decimal32_t(-12'345'675, 0), decimal32_t(-1'234'567, 1));

    // Edge case
    BOOST_TEST_EQ(decimal32_t(99'999'999), decimal32_t(1, 8));
}

int main()
{
    #ifndef BOOST_DECIMAL_NO_CONSTEVAL_DETECTION
    test_environment();
    test_constructor_rounding();
    #endif

    return boost::report_errors();
}
