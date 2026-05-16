// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// See: https://github.com/boostorg/decimal/issues/1091

#include <boost/decimal.hpp>
#include <boost/core/lightweight_test.hpp>

using namespace boost::decimal;

template <typename T>
void test()
{
    constexpr T one {1u, 0};
    T low {std::numeric_limits<T>::lowest()};
    BOOST_TEST(!isinf(low));

    low *= 2;

    BOOST_TEST(low < one);
    BOOST_TEST(one > low);
    BOOST_TEST(isinf(low));

    T high {std::numeric_limits<T>::max()};
    BOOST_TEST(!isinf(high));

    high *= 2;

    BOOST_TEST(high > one);
    BOOST_TEST(one < high);
    BOOST_TEST(isinf(high));

    T min {-std::numeric_limits<T>::denorm_min()};

    min /= 2;

    BOOST_TEST(min < one);
    BOOST_TEST(min == (one - one));
    BOOST_TEST(signbit(min));
}

int main()
{
    test<decimal32_t>();
    test<decimal64_t>();
    test<decimal128_t>();

    return boost::report_errors();
}

