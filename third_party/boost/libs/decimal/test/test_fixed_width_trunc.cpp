// Copyright 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/decimal.hpp>
#include <boost/core/lightweight_test.hpp>
#include <limits>

using namespace boost::decimal;

template <typename T>
void test()
{
    constexpr T test_val {UINT32_C(123456), 0};
    constexpr T validation_val_1 {UINT32_C(1), 5};
    constexpr T validation_val_2 {UINT32_C(12), 4};
    constexpr T validation_val_3 {UINT32_C(123), 3};
    constexpr T validation_val_4 {UINT32_C(1234), 2};
    constexpr T validation_val_5 {UINT32_C(12346), 1};

    BOOST_TEST_EQ(rescale(test_val, 0), rescale(test_val));
    BOOST_TEST_EQ(rescale(test_val, 1), validation_val_1);
    BOOST_TEST_EQ(rescale(test_val, 2), validation_val_2);
    BOOST_TEST_EQ(rescale(test_val, 3), validation_val_3);
    BOOST_TEST_EQ(rescale(test_val, 4), validation_val_4);
    BOOST_TEST_EQ(rescale(test_val, 5), validation_val_5);
    BOOST_TEST_EQ(rescale(test_val, 100), test_val);

    // Non-finite values
    for (int i = 0; i < 10; ++i)
    {
        BOOST_TEST(isinf(rescale(std::numeric_limits<T>::infinity(), i)));
        BOOST_TEST(isnan(rescale(std::numeric_limits<T>::quiet_NaN(), i)));
        BOOST_TEST(isnan(rescale(std::numeric_limits<T>::signaling_NaN(), i)));
        BOOST_TEST_EQ(rescale(T{0}, i), T{0});
    }

    // Big value
    constexpr T big_val {1, 20};
    for (int i = 0; i < 10; ++i)
    {
        BOOST_TEST_EQ(rescale(big_val, i), big_val);
    }
}

int main()
{
    test<decimal32_t>();
    test<decimal64_t>();
    test<decimal128_t>();

    test<decimal_fast32_t>();

    return boost::report_errors();
}
