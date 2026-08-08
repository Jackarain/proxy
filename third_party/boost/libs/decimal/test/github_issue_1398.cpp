// Copyright 2026 Chris Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// See: https://github.com/boostorg/decimal/issues/1398

#include <boost/decimal.hpp>
#include <boost/core/lightweight_test.hpp>
#include <limits>

template <typename T, typename U>
void test_driver()
{
    using namespace boost::decimal;

    constexpr auto builtin_neg_inf = -std::numeric_limits<T>::infinity();

    const auto val = U(builtin_neg_inf);
    BOOST_TEST(isinf(val));
    BOOST_TEST(val < 0);
}

template <typename T>
void test()
{
    using namespace boost::decimal;

    test_driver<T, decimal32_t>();
    test_driver<T, decimal64_t>();
    test_driver<T, decimal128_t>();
    test_driver<T, decimal_fast32_t>();
    test_driver<T, decimal_fast64_t>();
    test_driver<T, decimal_fast128_t>();
}

int main()
{
    test<float>();
    test<double>();

    return boost::report_errors();
}
