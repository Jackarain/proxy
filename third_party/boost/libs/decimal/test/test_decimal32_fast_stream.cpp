// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include "mini_to_chars.hpp"
#include <boost/decimal.hpp>
#include <boost/core/lightweight_test.hpp>
#include <iostream>
#include <iomanip>
#include <sstream>

using namespace boost::decimal;

void test_istream()
{
    decimal_fast32_t val;
    std::stringstream in;
    in.str("+1.234567e+06");
    in >> val;
    BOOST_TEST_EQ(val, decimal_fast32_t(1234567, 0));

    errno = 0;
    decimal_fast32_t val2;
    std::stringstream in_zero;
    in_zero.str("0");
    in_zero >> val2;
    BOOST_TEST_EQ(val2, decimal_fast32_t(0, 0)) && BOOST_TEST_EQ(errno, 0);

    decimal_fast32_t val3;
    std::stringstream bad;
    bad.str("");
    bad >> val3;
    BOOST_TEST_EQ(errno, EINVAL) && BOOST_TEST_NE(val3, std::numeric_limits<decimal_fast32_t>::signaling_NaN());

    errno = 0;
    decimal_fast32_t inf_val;
    std::stringstream inf;
    inf.str("inf");
    inf >> inf_val;
    BOOST_TEST_EQ(inf_val, std::numeric_limits<decimal_fast32_t>::infinity()) && BOOST_TEST_EQ(errno, 0);

    errno = 0;
    decimal_fast32_t inf_val2;
    std::stringstream inf2;
    inf2.str("INFINITY");
    inf2 >> inf_val2;
    BOOST_TEST_EQ(inf_val2, std::numeric_limits<decimal_fast32_t>::infinity()) && BOOST_TEST_EQ(errno, 0);

    errno = 0;
    decimal_fast32_t snan_val;
    std::stringstream snan;
    snan.str("-nan(snan)");
    snan >> snan_val;
    BOOST_TEST_NE(snan_val, std::numeric_limits<decimal_fast32_t>::signaling_NaN()) && BOOST_TEST_EQ(errno, 0);

    errno = 0;
    decimal_fast32_t nan_val;
    std::stringstream nan_str;
    nan_str.str("nan");
    nan_str >> nan_val;
    BOOST_TEST_NE(nan_val, std::numeric_limits<decimal_fast32_t>::quiet_NaN()) && BOOST_TEST_EQ(errno, 0);

    errno = 0;
    decimal_fast32_t junk_val;
    std::stringstream junk_str;
    junk_str.str("r5");
    junk_str >> junk_val;
    BOOST_TEST_NE(junk_val, std::numeric_limits<decimal_fast32_t>::signaling_NaN()) && BOOST_TEST_EQ(errno, EINVAL);
}

void test_ostream()
{
    decimal_fast32_t val {123456, 0};
    std::stringstream out;
    out << val;
    BOOST_TEST_CSTR_EQ(out.str().c_str(), "123456");

    // Tests the default value of setprecision
    decimal_fast32_t big_val {123456789, 0};
    std::stringstream big_out;
    big_out << big_val;
    BOOST_TEST_CSTR_EQ(big_out.str().c_str(), "1.234568e+08");

    decimal_fast32_t zero {0, 0};
    std::stringstream zero_out;
    zero_out << zero;
    BOOST_TEST_CSTR_EQ(zero_out.str().c_str(), "0");

    std::stringstream inf;
    inf << std::numeric_limits<decimal_fast32_t>::infinity();
    BOOST_TEST_CSTR_EQ(inf.str().c_str(), "inf");

    std::stringstream qnan;
    qnan << std::numeric_limits<decimal_fast32_t>::quiet_NaN();
    BOOST_TEST_CSTR_EQ(qnan.str().c_str(), "nan");

    std::stringstream snan;
    snan << std::numeric_limits<decimal_fast32_t>::signaling_NaN();
    BOOST_TEST_CSTR_EQ(snan.str().c_str(), "nan(snan)");

    std::stringstream neg_inf;
    neg_inf << (-std::numeric_limits<decimal_fast32_t>::infinity());
    BOOST_TEST_CSTR_EQ(neg_inf.str().c_str(), "-inf");

    std::stringstream neg_qnan;
    neg_qnan << (-std::numeric_limits<decimal_fast32_t>::quiet_NaN());
    BOOST_TEST_CSTR_EQ(neg_qnan.str().c_str(), "-nan(ind)");

    std::stringstream neg_snan;
    neg_snan << (-std::numeric_limits<decimal_fast32_t>::signaling_NaN());
    BOOST_TEST_CSTR_EQ(neg_snan.str().c_str(), "-nan(snan)");
}

#ifndef BOOST_DECIMAL_DISABLE_EXCEPTIONS

void test_issue_1127_locales(const char* locale)
{
    try
    {
        const std::locale a(locale);
        
        std::stringstream out_double;
        out_double.imbue(a);
        out_double << 1122.89;

        constexpr decimal_fast32_t val4{ 112289, -2 };
        std::stringstream out_decimal;
        out_decimal.imbue(a);
        out_decimal << val4;

        BOOST_TEST_CSTR_EQ(out_decimal.str().c_str(), out_double.str().c_str());
    }
    catch (...)
    {
        std::cerr << "Locale not installed. Skipping test." << std::endl;
        return;
    }
}

void test_locales()
{
    const char buffer[] = "1,1897e+02";

    try
    {
        #ifdef BOOST_MSVC
        std::locale::global(std::locale("German"));
        #else
        std::locale::global(std::locale("de_DE.UTF-8"));
        #endif
    }
        // LCOV_EXCL_START
    catch (...)
    {
        std::cerr << "Locale not installed. Skipping test." << std::endl;
        return;
    }
    // LCOV_EXCL_STOP

    std::stringstream in;
    in.str(buffer);
    decimal_fast32_t val;
    in >> val;
    BOOST_TEST_EQ(val, decimal_fast32_t(1.1897e+02));

    std::stringstream out;
    out << std::scientific << std::setprecision(4) << val;
    BOOST_TEST_CSTR_EQ(out.str().c_str(), buffer);
}

#endif

int main()
{
    test_istream();
    test_ostream();

    // Homebrew GCC does not support locales
    #if !(defined(__GNUC__) && __GNUC__ >= 8 && defined(__APPLE__)) && !defined(BOOST_DECIMAL_QEMU_TEST) && !defined(BOOST_DECIMAL_DISABLE_EXCEPTIONS)
    #ifndef _MSC_VER
    test_issue_1127_locales("en_US.UTF-8"); // . decimal, , thousands
    test_issue_1127_locales("de_DE.UTF-8"); // , decimal, . thousands
    #if (defined(__clang__) && __clang_major__ > 9) || (defined(__GNUC__) && __GNUC__ > 9)
    test_issue_1127_locales("fr_FR.UTF-8"); // , decimal, . thousands
    #endif
    #endif
    test_locales();
    #endif

    return boost::report_errors();
}
