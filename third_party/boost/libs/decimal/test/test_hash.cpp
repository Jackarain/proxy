// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// This is a toy example to make sure that the hashing compiles correctly

#include <boost/decimal/decimal32_t.hpp>
#include <boost/decimal/decimal64_t.hpp>
#include <boost/decimal/decimal128_t.hpp>
#include <boost/decimal/decimal_fast32_t.hpp>
#include <boost/decimal/decimal_fast64_t.hpp>
#include <boost/decimal/decimal_fast128_t.hpp>
#include <boost/decimal/hash.hpp>
#include <boost/decimal/iostream.hpp>
#include <boost/core/lightweight_test.hpp>
#include <functional>
#include <array>

template <typename T>
void test_hash()
{
    std::hash<T> hasher;
    for (int i = 0; i < 100; ++i)
    {
        T dec_val(i);
        BOOST_TEST_EQ(hasher(dec_val), hasher(dec_val));
    }
}

// See: https://github.com/boostorg/decimal/issues/1120
template <typename T>
void test_hash_cohorts()
{
    const std::array<T, 7> values {
        T {3, 7},
        T {30, 6},
        T {300, 5},
        T {3000, 4},
        T {30000, 3},
        T {300000, 2},
        T {3000000, 1}
    };

    std::hash<T> hasher;

    for (const auto val1 : values)
    {
        for (const auto val2 : values)
        {
            BOOST_TEST_EQ(hasher(val1), hasher(val2));
        }
    }
}

int main()
{
    test_hash<boost::decimal::decimal32_t>();
    test_hash<boost::decimal::decimal64_t>();
    test_hash<boost::decimal::decimal128_t>();
    test_hash<boost::decimal::decimal_fast32_t>();
    test_hash<boost::decimal::decimal_fast64_t>();
    test_hash<boost::decimal::decimal_fast128_t>();

    test_hash_cohorts<boost::decimal::decimal32_t>();
    test_hash_cohorts<boost::decimal::decimal64_t>();
    test_hash_cohorts<boost::decimal::decimal128_t>();
    test_hash_cohorts<boost::decimal::decimal_fast32_t>();
    test_hash_cohorts<boost::decimal::decimal_fast64_t>();
    test_hash_cohorts<boost::decimal::decimal_fast128_t>();

    return boost::report_errors();
}
