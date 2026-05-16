// Copyright 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/decimal.hpp>
#include <boost/core/lightweight_test.hpp>
#include <random>

using namespace boost::decimal;

template <typename T>
void test()
{
    std::mt19937_64 rng(42);
    std::uniform_int_distribution<std::int64_t> dist(std::numeric_limits<std::int64_t>::min(),
                                                     std::numeric_limits<std::int64_t>::max());

    for (std::size_t i {}; i < 1024; ++i)
    {
        const T val {dist(rng)};
        const auto bits {to_bid<T>(val)};
        const T return_val {from_bid<T>(bits)};
        BOOST_TEST_EQ(val, return_val);
    }
}

int main()
{
    test<decimal_fast32_t>();
    test<decimal_fast64_t>();

    test<decimal32_t>();
    test<decimal64_t>();

    test<decimal128_t>();
    test<decimal_fast128_t>();

    return boost::report_errors();
}
