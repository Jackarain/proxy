// Copyright 2025 Christopher Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// See: https://github.com/boostorg/decimal/issues/1110

#include <boost/decimal/decimal128_t.hpp>
#include <boost/decimal/cmath.hpp>
#include <boost/decimal/iostream.hpp>

#include <boost/core/lightweight_test.hpp>

#include <iomanip>
#include <iostream>
#include <sstream>

using namespace boost::decimal;

namespace local {

auto test() -> void;

auto test() -> void
{
    const boost::decimal::decimal128_t one { 1 };
    const boost::decimal::decimal128_t del { 1, -32 };
    const boost::decimal::decimal128_t sum { one + del };

    const boost::decimal::decimal128_t sqr { sqrt(sum) };

    {
        std::stringstream strm { };

        strm << std::setprecision(std::numeric_limits<boost::decimal::decimal128_t>::digits10) << sqr;

        BOOST_TEST_CSTR_EQ(strm.str().c_str(), "1.000000000000000000000000000000005");
    }

    const boost::decimal::decimal128_t cbr { cbrt(sum) };

    {
        std::stringstream strm { };

        strm << std::setprecision(std::numeric_limits<boost::decimal::decimal128_t>::digits10)<< cbr;

        BOOST_TEST_CSTR_EQ(strm.str().c_str(), "1.000000000000000000000000000000003");
    }

    const boost::decimal::decimal128_t lgt { log10(sum) };

    {
        std::stringstream strm { };

        strm << std::setprecision(std::numeric_limits<boost::decimal::decimal128_t>::digits10)<< lgt;

        BOOST_TEST_CSTR_EQ(strm.str().c_str(), "4.4e-33");
    }
}

} // namespace local

auto main() -> int
{
    local::test();

    return boost::report_errors();
}
