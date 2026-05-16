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

using namespace boost::decimal;

template <typename Dec>
void tiny_div()
{
    constexpr Dec zero {0};

    const auto tiny = std::numeric_limits<Dec>::denorm_min();
    const auto tiny2 = tiny / Dec{10};
    BOOST_TEST(tiny2 == zero);
}

int main()
{
    tiny_div<decimal32_t>();
    tiny_div<decimal64_t>();
    tiny_div<decimal128_t>();

    tiny_div<decimal_fast32_t>();
    tiny_div<decimal_fast64_t>();
    tiny_div<decimal_fast128_t>();

    return boost::report_errors();
}
