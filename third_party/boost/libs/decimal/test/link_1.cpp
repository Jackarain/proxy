// Copyright 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/decimal.hpp>

void test_odr_use(const boost::decimal::rounding_mode*);

// LCOV_EXCL_START
template <typename T>
void test()
{
    test_odr_use(&boost::decimal::_boost_decimal_global_rounding_mode);
}

void f1()
{
    test<boost::decimal::decimal32_t>();
    test<boost::decimal::decimal64_t>();
    test<boost::decimal::decimal128_t>();
}
// LCOV_EXCL_STOP
