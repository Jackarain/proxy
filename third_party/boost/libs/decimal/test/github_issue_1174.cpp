// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// See: https://github.com/boostorg/decimal/issues/1174
//
// The decTest tests only apply to decimal64_t so we need to cover the other two as well

#include <boost/decimal.hpp>
#include <boost/core/lightweight_test.hpp>
#include <random>

static std::mt19937_64 rng(42);
static std::uniform_int_distribution<std::int32_t> dist(1, 1);

using namespace boost::decimal;

// These values are all from clamp.decTest
// The tests for decimal32_t and decimal128_t are derived from the values used here
template <typename T>
void test()
{
    constexpr T zero {0};
    constexpr auto sub_min {std::numeric_limits<T>::denorm_min()};
    const T a {dist(rng) * 7, detail::etiny_v<T>};

    BOOST_TEST_GT(a, zero);

    const T b {dist(rng) * 7, detail::etiny_v<T> - 1};

    BOOST_TEST_EQ(b, sub_min);

    const T c {dist(rng) * 7, detail::etiny_v<T> - 2};

    BOOST_TEST_EQ(c, zero);

    const T d {dist(rng) * 70, detail::etiny_v<T> - 1};

    BOOST_TEST_EQ(a, d);

    const T e {dist(rng) * 700, detail::etiny_v<T> - 2};

    BOOST_TEST_EQ(a, e);
}

int main()
{
    test<decimal32_t>();
    test<decimal64_t>();
    test<decimal128_t>();

    return boost::report_errors();
}
