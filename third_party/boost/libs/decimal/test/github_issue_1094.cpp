// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// See: https://github.com/boostorg/decimal/issues/1094

#include <boost/decimal.hpp>
#include <boost/core/lightweight_test.hpp>

using namespace boost::decimal;

int main()
{
    BOOST_TEST(boost::decimal::fegetround() == _boost_decimal_global_rounding_mode);
    BOOST_TEST(boost::decimal::fegetround() == _boost_decimal_global_runtime_rounding_mode);

    #ifndef BOOST_DECIMAL_NO_CONSTEVAL_DETECTION

    BOOST_TEST(fesetround(rounding_mode::fe_dec_upward) == rounding_mode::fe_dec_upward);
    BOOST_TEST(boost::decimal::fegetround() != _boost_decimal_global_rounding_mode);
    BOOST_TEST(boost::decimal::fegetround() == _boost_decimal_global_runtime_rounding_mode);
    BOOST_TEST(boost::decimal::fegetround() == rounding_mode::fe_dec_upward);

    #else

    BOOST_TEST(fesetround(rounding_mode::fe_dec_upward) == _boost_decimal_global_rounding_mode);
    BOOST_TEST(boost::decimal::fegetround() == _boost_decimal_global_rounding_mode);
    BOOST_TEST(boost::decimal::fegetround() == _boost_decimal_global_runtime_rounding_mode);
    BOOST_TEST(boost::decimal::fegetround() == rounding_mode::fe_dec_default);

    #endif

    return boost::report_errors();
}
