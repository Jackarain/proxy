// Copyright 2026 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// See: https://github.com/boostorg/decimal/issues/1306

#include <boost/decimal.hpp>
#include <boost/core/lightweight_test.hpp>
#include <limits>

using namespace boost::decimal;

template <typename T>
void test()
{
    const T downward {13, detail::etiny_v<T> + 3};
    BOOST_TEST_EQ(downward * T{"1e-4"}, std::numeric_limits<T>::denorm_min());

    const T upward {15, detail::etiny_v<T> + 3};
    BOOST_TEST_EQ(upward * T{"1e-4"}, T(2, detail::etiny_v<T>));

    const T non_rounded {1234, detail::etiny_v<T> - 3};
    BOOST_TEST_EQ(non_rounded, std::numeric_limits<T>::denorm_min());

    const T rounded {1999, detail::etiny_v<T> - 3};
    BOOST_TEST_EQ(rounded, T(2, detail::etiny_v<T>));
}

int main()
{
    test<decimal32_t>();
    test<decimal64_t>();
    test<decimal128_t>();

    return boost::report_errors();
}
