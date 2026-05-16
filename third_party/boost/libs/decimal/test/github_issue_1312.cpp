// Copyright 2026 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// See: https://github.com/boostorg/decimal/issues/1312

#include <boost/decimal.hpp>
#include <boost/core/lightweight_test.hpp>
#include <limits>

using namespace boost::decimal;

template <typename T>
void test()
{
    constexpr T inf {std::numeric_limits<T>::infinity()};

    BOOST_TEST(isnan(inf / -inf));
    BOOST_TEST_EQ(inf / -1000, -inf);
    BOOST_TEST_EQ(inf / -T{1000}, -inf);
    BOOST_TEST_EQ(inf / 0, inf);
    BOOST_TEST_EQ(inf / T{0}, inf);
    BOOST_TEST_EQ(0 / inf, T{0});
}

int main()
{
    test<decimal32_t>();
    test<decimal64_t>();
    test<decimal128_t>();

    test<decimal_fast32_t>();
    test<decimal_fast64_t>();
    test<decimal_fast128_t>();

    return boost::report_errors();
}
