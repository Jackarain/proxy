// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// See: https://github.com/boostorg/decimal/issues/1106

#include <boost/decimal.hpp>
#include <boost/core/lightweight_test.hpp>
#include <limits>
#include <cmath>

using namespace boost::decimal;

template <typename T>
void test()
{
    using std::isinf;
    using std::signbit;

    const auto a {std::numeric_limits<T>::lowest() / 99};
    const T b {100};
    const auto c {a * b};

    BOOST_TEST(isinf(c));
    BOOST_TEST(signbit(c));

    // The same as above in principle 
    const auto d {a / (1/b)};
    BOOST_TEST(isinf(d));
    BOOST_TEST(signbit(d));
}

int main()
{
    #ifndef _MSC_VER
    test<float>();
    test<double>();
    #endif

    test<decimal32_t>();
    test<decimal64_t>();
    test<decimal128_t>();

    test<decimal_fast32_t>();
    test<decimal_fast64_t>();
    test<decimal_fast128_t>();

    return boost::report_errors();
}
