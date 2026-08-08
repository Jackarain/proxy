// Copyright 2026 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// See: https://github.com/boostorg/decimal/issues/1383

#include <boost/decimal.hpp>
#include <boost/core/lightweight_test.hpp>

using namespace boost::decimal;

template <typename T>
void test_spot(const T lhs, const T rhs, const T result)
{
    const auto q {quantize(lhs, rhs)};
    BOOST_TEST_EQ(q, result);
    // Per IEEE 754-2008 5.3.2 quantize: the result has the quantum of rhs.
    BOOST_TEST(samequantum(q, rhs));
}

template <typename T>
void test_driver()
{
    test_spot(T{0}, T{1}, T{0});
    test_spot(T{1}, T{1}, T{1});
    test_spot(T{1, -1}, T{1, 2}, T{0, 2});
    test_spot(T{1, -1}, T{1, 1}, T{0, 1});
    test_spot(T{1, -1}, T{1, -1}, T{1, -1});

    // Also exercise IEEE 754-2008 3.5.1 cohort preservation directly: zero with
    // distinct exponents must not collapse to a single canonical representation.
    const T zero_neg3 {0, -3};
    const T zero_pos5 {0, 5};
    const T one_neg3 {1, -3};
    BOOST_TEST(samequantum(zero_neg3, one_neg3));
    BOOST_TEST(!samequantum(zero_neg3, zero_pos5));
    BOOST_TEST_EQ(zero_neg3, zero_pos5);
}

int main()
{
    test_driver<decimal32_t>();
    test_driver<decimal64_t>();
    test_driver<decimal128_t>();

    return boost::report_errors();
}
