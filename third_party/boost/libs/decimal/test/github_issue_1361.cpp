// Copyright 2026 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// See: https://github.com/boostorg/decimal/issues/1319

#include <boost/decimal.hpp>
#include <boost/core/lightweight_test.hpp>
#include <limits>

using namespace boost::decimal;

#if !(defined(__clang__) && __clang_major__ < 9)

template <typename T>
void test()
{
    constexpr T zero {0};
    constexpr T numerator {1, -20};
    constexpr T res {"3.1830988618379067153776752674502872406891929148091289749533e-21"}; // Wolfram
    const auto val {numerator / numbers::pi_v<T>};
    BOOST_TEST_NE(zero, val);
    BOOST_TEST_EQ(val, res);
    const auto res_as_unsigned {static_cast<unsigned>(res)};
    BOOST_TEST_EQ(0U, res_as_unsigned);
    BOOST_TEST_EQ(0U, static_cast<unsigned>(val));

    constexpr T small_angle {1, -4};
    const T sin_small_angle {sin(small_angle)};
    // Approx: 0.00009999999983333334
    BOOST_TEST_GE(small_angle, sin_small_angle);
    BOOST_TEST_LE(small_angle / 10U, sin_small_angle);

    constexpr T smaller_angle {1, -20};
    BOOST_TEST_EQ(smaller_angle, sin(smaller_angle));
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

#else

int main() { return 0; }

#endif
