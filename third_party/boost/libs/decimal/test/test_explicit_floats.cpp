// Copyright 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/decimal.hpp>

using namespace boost::decimal;

template <typename T>
void test()
{
    constexpr T half {5, -1};

    const auto temp1 = half + 1.0;
    static_cast<void>(temp1);
    const auto temp2 = half > 1.0;
    static_cast<void>(temp2);
    const auto temp3 = half / 1.0;
    static_cast<void>(temp3);
}

int main()
{
    test<decimal32_t>();
    test<decimal64_t>();
    test<decimal128_t>();

    return 0;
}
