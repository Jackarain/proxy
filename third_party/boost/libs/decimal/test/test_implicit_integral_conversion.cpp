// Copyright 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

// Needed for operations with boost math
#define BOOST_DECIMAL_ALLOW_IMPLICIT_INTEGER_CONVERSIONS

#include <boost/decimal.hpp>
#include <boost/core/lightweight_test.hpp>
#include <random>

static std::mt19937_64 rng(42);

template <typename T>
void test()
{
    std::uniform_int_distribution<std::int64_t> dist(-9'999'999, 9'999'999);

    for (std::size_t i {}; i < 1024U; ++i)
    {
        const std::int64_t val {dist(rng)};
        T dec_val = val;
        BOOST_TEST_EQ(static_cast<std::int64_t>(dec_val), val);
    }
}

int main()
{
    test<boost::decimal::decimal32_t>();
    test<boost::decimal::decimal64_t>();
    test<boost::decimal::decimal128_t>();

    return boost::report_errors();
}
