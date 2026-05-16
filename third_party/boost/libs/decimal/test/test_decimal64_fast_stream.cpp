// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include "mini_to_chars.hpp"
#include <boost/decimal.hpp>
#include <boost/core/lightweight_test.hpp>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cerrno>

using namespace boost::decimal;

void test_istream()
{
    decimal_fast64_t val;
    std::stringstream in;
    in.str("+1.234567e+06");
    in >> val;
    BOOST_TEST_EQ(val, decimal_fast64_t(1234567, 0));

    errno = 0;
    decimal_fast64_t val2;
    std::stringstream in_zero;
    in_zero.str("0");
    in_zero >> val2;
    BOOST_TEST_EQ(val2, decimal_fast64_t(0, 0)) && BOOST_TEST_EQ(errno, 0);

    decimal_fast64_t val3;
    std::stringstream bad;
    bad.str("");
    bad >> val3;
    BOOST_TEST_EQ(errno, EINVAL) && BOOST_TEST_NE(val3, std::numeric_limits<decimal_fast64_t>::signaling_NaN());

    errno = 0;
    decimal_fast64_t inf_val;
    std::stringstream inf;
    inf.str("inf");
    inf >> inf_val;
    BOOST_TEST_EQ(inf_val, std::numeric_limits<decimal_fast64_t>::infinity()) && BOOST_TEST_EQ(errno, 0);

    errno = 0;
    decimal_fast64_t inf_val2;
    std::stringstream inf2;
    inf2.str("INFINITY");
    inf2 >> inf_val2;
    BOOST_TEST_EQ(inf_val2, std::numeric_limits<decimal_fast64_t>::infinity()) && BOOST_TEST_EQ(errno, 0);

    errno = 0;
    decimal_fast64_t snan_val;
    std::stringstream snan;
    snan.str("-nan(snan)");
    snan >> snan_val;
    BOOST_TEST_NE(snan_val, std::numeric_limits<decimal_fast64_t>::signaling_NaN()) && BOOST_TEST_EQ(errno, 0);

    errno = 0;
    decimal_fast64_t nan_val;
    std::stringstream nan_str;
    nan_str.str("nan");
    nan_str >> nan_val;
    BOOST_TEST_NE(nan_val, std::numeric_limits<decimal_fast64_t>::quiet_NaN()) && BOOST_TEST_EQ(errno, 0);

    errno = 0;
    decimal_fast64_t junk_val;
    std::stringstream junk_str;
    junk_str.str("r5");
    junk_str >> junk_val;
    BOOST_TEST_NE(junk_val, std::numeric_limits<decimal_fast64_t>::signaling_NaN()) && BOOST_TEST_EQ(errno, EINVAL);
}

void test_ostream()
{
    decimal_fast64_t val {123456, 0};
    std::stringstream out;
    out << val;
    BOOST_TEST_CSTR_EQ(out.str().c_str(), "123456");

    decimal_fast64_t zero {0, 0};
    std::stringstream zero_out;
    zero_out << zero;
    BOOST_TEST_CSTR_EQ(zero_out.str().c_str(), "0");

    std::stringstream inf;
    inf << std::numeric_limits<decimal_fast64_t>::infinity();
    BOOST_TEST_CSTR_EQ(inf.str().c_str(), "inf");

    std::stringstream qnan;
    qnan << std::numeric_limits<decimal_fast64_t>::quiet_NaN();
    BOOST_TEST_CSTR_EQ(qnan.str().c_str(), "nan");

    std::stringstream snan;
    snan << std::numeric_limits<decimal_fast64_t>::signaling_NaN();
    BOOST_TEST_CSTR_EQ(snan.str().c_str(), "nan(snan)");

    std::stringstream neg_inf;
    neg_inf << (-std::numeric_limits<decimal_fast64_t>::infinity());
    BOOST_TEST_CSTR_EQ(neg_inf.str().c_str(), "-inf");

    std::stringstream neg_qnan;
    neg_qnan << (-std::numeric_limits<decimal_fast64_t>::quiet_NaN());
    BOOST_TEST_CSTR_EQ(neg_qnan.str().c_str(), "-nan(ind)");

    std::stringstream neg_snan;
    neg_snan << (-std::numeric_limits<decimal_fast64_t>::signaling_NaN());
    BOOST_TEST_CSTR_EQ(neg_snan.str().c_str(), "-nan(snan)");
}

int main()
{
    test_istream();
    test_ostream();

    return boost::report_errors();
}
