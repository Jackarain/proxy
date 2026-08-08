// Copyright 2026 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// See: https://github.com/boostorg/decimal/issues/1378

#include <boost/decimal.hpp>
#include <boost/core/lightweight_test.hpp>

using namespace boost::decimal;

template <typename T>
void test_floor()
{
    BOOST_TEST_EQ(T{-125}, floor(T{-125}));
    BOOST_TEST_EQ(T{-100}, floor(T{-100}));
    BOOST_TEST_EQ(T{-1}, floor(T{-1}));
    BOOST_TEST_EQ(T{0}, floor(T{0}));
    BOOST_TEST_EQ(T{1}, floor(T{1}));
    BOOST_TEST_EQ(T{100}, floor(T{100}));
    BOOST_TEST_EQ(T{125}, floor(T{125}));
}

template <typename T>
void test_ceil()
{
    BOOST_TEST_EQ(T{-125}, ceil(T{-125}));
    BOOST_TEST_EQ(T{-100}, ceil(T{-100}));
    BOOST_TEST_EQ(T{-1}, ceil(T{-1}));
    BOOST_TEST_EQ(T{0}, ceil(T{0}));
    BOOST_TEST_EQ(T{1}, ceil(T{1}));
    BOOST_TEST_EQ(T{100}, ceil(T{100}));
    BOOST_TEST_EQ(T{125}, ceil(T{125}));
}

int main()
{
    test_floor<decimal32_t>();
    test_floor<decimal64_t>();
    test_floor<decimal128_t>();

    test_ceil<decimal32_t>();
    test_ceil<decimal64_t>();
    test_ceil<decimal128_t>();

    return boost::report_errors();
}
