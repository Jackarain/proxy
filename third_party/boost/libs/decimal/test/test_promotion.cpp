// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/decimal.hpp>
#include <boost/decimal/detail/promotion.hpp>
#include <type_traits>
#include <cstdint>

using namespace boost::decimal;

int main()
{
    static_assert(std::is_same<detail::promote_args_t<char>, double>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<unsigned char>, double>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<int>, double>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<unsigned>, double>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<long>, double>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<unsigned long>, double>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<long long>, double>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<unsigned long long>, double>::value, "False");

    static_assert(std::is_same<detail::promote_args_t<float>, float>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<double>, double>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<long double>, long double>::value, "False");

    static_assert(std::is_same<detail::promote_args_t<char, double>, double>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<unsigned char, double>, double>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<int, double>, double>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<unsigned, double>, double>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<long, double>, double>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<unsigned long, double>, double>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<long long, double>, double>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<unsigned long long, double>, double>::value, "False");

    static_assert(std::is_same<detail::promote_args_t<float, double>, double>::value, "False");

    static_assert(std::is_same<detail::promote_args_t<char, decimal32_t>, decimal32_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<unsigned char, decimal32_t>, decimal32_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<int, decimal32_t>, decimal32_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<unsigned, decimal32_t>, decimal32_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<long, decimal32_t>, decimal32_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<unsigned long, decimal32_t>, decimal32_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<long long, decimal32_t>, decimal32_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<unsigned long long, decimal32_t>, decimal32_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<float, decimal32_t>, decimal32_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<double, decimal32_t>, decimal32_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<long double, decimal32_t>, decimal32_t>::value, "False");

    static_assert(std::is_same<detail::promote_args_t<char, double, decimal32_t>, decimal32_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<unsigned char, double, decimal32_t>, decimal32_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<int, double, decimal32_t>, decimal32_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<unsigned, double, decimal32_t>, decimal32_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<long, double, decimal32_t>, decimal32_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<unsigned long, double, decimal32_t>, decimal32_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<long long, double, decimal32_t>, decimal32_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<unsigned long long, double, decimal32_t>, decimal32_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<float, double, decimal32_t>, decimal32_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<double, double, decimal32_t>, decimal32_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<long double, double, decimal32_t>, decimal32_t>::value, "False");

    static_assert(std::is_same<detail::promote_args_t<char, decimal64_t>, decimal64_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<unsigned char, decimal64_t>, decimal64_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<int, decimal64_t>, decimal64_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<unsigned, decimal64_t>, decimal64_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<long, decimal64_t>, decimal64_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<unsigned long, decimal64_t>, decimal64_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<long long, decimal64_t>, decimal64_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<unsigned long long, decimal64_t>, decimal64_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<float, decimal64_t>, decimal64_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<double, decimal64_t>, decimal64_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<long double, decimal64_t>, decimal64_t>::value, "False");

    static_assert(std::is_same<detail::promote_args_t<char, double, decimal64_t>, decimal64_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<unsigned char, double, decimal64_t>, decimal64_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<int, double, decimal64_t>, decimal64_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<unsigned, double, decimal64_t>, decimal64_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<long, double, decimal64_t>, decimal64_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<unsigned long, double, decimal64_t>, decimal64_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<long long, double, decimal64_t>, decimal64_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<unsigned long long, double, decimal64_t>, decimal64_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<float, double, decimal64_t>, decimal64_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<double, double, decimal64_t>, decimal64_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<long double, double, decimal64_t>, decimal64_t>::value, "False");

    static_assert(std::is_same<detail::promote_args_t<decimal32_t, decimal32_t>, decimal32_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<decimal64_t, decimal64_t>, decimal64_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<decimal32_t, decimal64_t>, decimal64_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<decimal64_t, decimal32_t>, decimal64_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<decimal32_t, decimal32_t, decimal64_t>, decimal64_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<decimal32_t, decimal32_t, decimal32_t>, decimal32_t>::value, "False");

    static_assert(std::is_same<detail::promote_args_t<decimal32_t, decimal_fast32_t>, decimal_fast32_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<decimal_fast32_t, decimal32_t>, decimal_fast32_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<decimal64_t, decimal_fast32_t>, decimal64_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<decimal_fast32_t, decimal64_t>, decimal64_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<decimal128_t, decimal_fast32_t>, decimal128_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<decimal_fast32_t, decimal128_t>, decimal128_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<decimal32_t, decimal_fast32_t, decimal64_t>, decimal64_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<decimal_fast32_t, decimal32_t, decimal_fast32_t>, decimal_fast32_t>::value, "False");

    static_assert(std::is_same<detail::promote_args_t<decimal32_t, decimal_fast64_t>, decimal_fast64_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<decimal_fast64_t, decimal32_t>, decimal_fast64_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<decimal64_t, decimal_fast64_t>, decimal_fast64_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<decimal_fast64_t, decimal64_t>, decimal_fast64_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<decimal128_t, decimal_fast64_t>, decimal128_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<decimal_fast64_t, decimal128_t>, decimal128_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<decimal32_t, decimal_fast64_t, decimal64_t>, decimal_fast64_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<decimal_fast64_t, decimal32_t, decimal_fast64_t>, decimal_fast64_t>::value, "False");

    static_assert(std::is_same<detail::promote_args_t<decimal32_t, decimal_fast128_t>, decimal_fast128_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<decimal_fast128_t, decimal32_t>, decimal_fast128_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<decimal64_t, decimal_fast128_t>, decimal_fast128_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<decimal_fast128_t, decimal64_t>, decimal_fast128_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<decimal128_t, decimal_fast128_t>, decimal_fast128_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<decimal_fast128_t, decimal128_t>, decimal_fast128_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<decimal32_t, decimal_fast128_t, decimal64_t>, decimal_fast128_t>::value, "False");
    static_assert(std::is_same<detail::promote_args_t<decimal_fast128_t, decimal32_t, decimal_fast64_t>, decimal_fast128_t>::value, "False");

    return 0;
}
