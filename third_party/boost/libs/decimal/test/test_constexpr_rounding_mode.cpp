// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt


#define BOOST_DECIMAL_FE_DEC_DOWNWARD
#include <boost/decimal.hpp>
#include <boost/core/lightweight_test.hpp>

using namespace boost::decimal;

int main()
{
    BOOST_TEST(_boost_decimal_global_rounding_mode == rounding_mode::fe_dec_downward);
    BOOST_TEST(_boost_decimal_global_rounding_mode == _boost_decimal_global_runtime_rounding_mode);
    BOOST_TEST(boost::decimal::fegetround() == rounding_mode::fe_dec_downward);

    #ifndef BOOST_DECIMAL_NO_CONSTEVAL_DETECTION

    BOOST_TEST(boost::decimal::fesetround(rounding_mode::fe_dec_default) == rounding_mode::fe_dec_default);
    BOOST_TEST(_boost_decimal_global_rounding_mode == rounding_mode::fe_dec_downward);
    BOOST_TEST(_boost_decimal_global_rounding_mode != _boost_decimal_global_runtime_rounding_mode);
    BOOST_TEST(boost::decimal::fegetround() == rounding_mode::fe_dec_default);

    #endif

    return boost::report_errors();
}
