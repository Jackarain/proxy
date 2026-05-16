// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/decimal.hpp>
#include <boost/core/lightweight_test.hpp>

template <typename From, typename To>
void test_implicit()
{
    const From from_val {2, 1};
    const To to_val = from_val;

    BOOST_TEST_EQ(from_val, to_val);
}

template <typename From, typename To>
void test_explicit()
{
    const From from_val {2, 1};
    const To to_val = static_cast<To>(from_val);

    BOOST_TEST_EQ(from_val, to_val);
}

int main()
{
    test_explicit<boost::decimal::decimal32_t, boost::decimal::decimal32_t>();
    test_explicit<boost::decimal::decimal_fast32_t, boost::decimal::decimal32_t>();
    test_explicit<boost::decimal::decimal64_t, boost::decimal::decimal32_t>();
    test_explicit<boost::decimal::decimal_fast64_t, boost::decimal::decimal32_t>();
    test_explicit<boost::decimal::decimal128_t, boost::decimal::decimal32_t>();
    test_explicit<boost::decimal::decimal_fast128_t, boost::decimal::decimal32_t>();

    test_explicit<boost::decimal::decimal32_t, boost::decimal::decimal_fast32_t>();
    test_explicit<boost::decimal::decimal_fast32_t, boost::decimal::decimal_fast32_t>();
    test_explicit<boost::decimal::decimal64_t, boost::decimal::decimal_fast32_t>();
    test_explicit<boost::decimal::decimal_fast64_t, boost::decimal::decimal_fast32_t>();
    test_explicit<boost::decimal::decimal128_t, boost::decimal::decimal_fast32_t>();
    test_explicit<boost::decimal::decimal_fast128_t, boost::decimal::decimal_fast32_t>();

    test_explicit<boost::decimal::decimal32_t, boost::decimal::decimal_fast64_t>();
    test_explicit<boost::decimal::decimal_fast32_t, boost::decimal::decimal_fast64_t>();
    test_explicit<boost::decimal::decimal64_t, boost::decimal::decimal_fast64_t>();
    test_explicit<boost::decimal::decimal_fast64_t, boost::decimal::decimal_fast64_t>();
    test_explicit<boost::decimal::decimal128_t, boost::decimal::decimal_fast64_t>();
    test_explicit<boost::decimal::decimal_fast128_t, boost::decimal::decimal_fast64_t>();

    test_explicit<boost::decimal::decimal32_t, boost::decimal::decimal128_t>();
    test_explicit<boost::decimal::decimal_fast32_t, boost::decimal::decimal128_t>();
    test_explicit<boost::decimal::decimal64_t, boost::decimal::decimal128_t>();
    test_explicit<boost::decimal::decimal_fast64_t, boost::decimal::decimal128_t>();
    test_explicit<boost::decimal::decimal128_t, boost::decimal::decimal128_t>();
    test_explicit<boost::decimal::decimal_fast128_t, boost::decimal::decimal128_t>();

    test_implicit<boost::decimal::decimal32_t, boost::decimal::decimal32_t>();
    test_implicit<boost::decimal::decimal32_t, boost::decimal::decimal_fast32_t>();
    test_implicit<boost::decimal::decimal32_t, boost::decimal::decimal64_t>();
    test_implicit<boost::decimal::decimal32_t, boost::decimal::decimal_fast64_t>();
    test_implicit<boost::decimal::decimal32_t, boost::decimal::decimal128_t>();
    test_implicit<boost::decimal::decimal32_t, boost::decimal::decimal_fast128_t>();

    test_implicit<boost::decimal::decimal_fast32_t, boost::decimal::decimal_fast32_t>();
    test_implicit<boost::decimal::decimal_fast32_t, boost::decimal::decimal64_t>();
    test_implicit<boost::decimal::decimal_fast32_t, boost::decimal::decimal_fast64_t>();
    test_implicit<boost::decimal::decimal_fast32_t, boost::decimal::decimal128_t>();
    test_implicit<boost::decimal::decimal_fast32_t, boost::decimal::decimal_fast128_t>();

    test_implicit<boost::decimal::decimal64_t, boost::decimal::decimal64_t>();
    test_implicit<boost::decimal::decimal64_t, boost::decimal::decimal_fast64_t>();
    test_implicit<boost::decimal::decimal64_t, boost::decimal::decimal128_t>();
    test_implicit<boost::decimal::decimal64_t, boost::decimal::decimal_fast128_t>();

    test_implicit<boost::decimal::decimal_fast64_t, boost::decimal::decimal_fast64_t>();
    test_implicit<boost::decimal::decimal_fast64_t, boost::decimal::decimal128_t>();
    test_implicit<boost::decimal::decimal_fast64_t, boost::decimal::decimal_fast128_t>();

    test_implicit<boost::decimal::decimal128_t, boost::decimal::decimal128_t>();
    test_implicit<boost::decimal::decimal128_t, boost::decimal::decimal_fast128_t>();

    test_implicit<boost::decimal::decimal_fast128_t, boost::decimal::decimal_fast128_t>();

    return boost::report_errors();
}
