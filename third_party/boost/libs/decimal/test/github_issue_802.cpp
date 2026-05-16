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

void fasting()
{
    using namespace boost::decimal;
    using Dec = decimal128_t;
    const Dec a{100'000,1};  // 6 dec digits significant
    const Dec b{2'000'000,1}; // 7 dec digits significant
    constexpr Dec ab{2, 13};

    // Here we should be able to use the fast path instead of the slow path
    // since 2e13 fits in a 128 bit uint
    BOOST_TEST((a * b) == ab);
}

int main()
{
    fasting();

    return boost::report_errors();
}

