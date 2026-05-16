// Copyright 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#define BOOST_DECIMAL_ALLOW_IMPLICIT_CONVERSIONS

#include <boost/decimal.hpp>
#include <boost/core/lightweight_test.hpp>

#ifdef BOOST_DECIMAL_UNSUPPORTED_LONG_DOUBLE

int main()
{
    return 0;
}

#elif defined(__GNUC__) && __GNUC__ >= 5 && __cplusplus > 202002L

#include <complex>

int main()
{
    using namespace boost::decimal;

    constexpr decimal64_t half {5, -1};
    std::complex<decimal64_t> test_val {half, half};
    const auto res = std::acos(test_val);
    static_cast<void>(res);

    return 0;
}

#else

int main()
{
    using namespace boost::decimal;

    const decimal64_t test_val = 1.5707963267948966192313216916397514L;
    BOOST_TEST_EQ(test_val, decimal64_t{1.5707963267948966192313216916397514L});

    return boost::report_errors();
}

#endif
