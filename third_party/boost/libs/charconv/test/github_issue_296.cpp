// Copyright 2026 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// See: https://github.com/boostorg/charconv/issues/296

#include <boost/charconv.hpp>
#include <boost/core/lightweight_test.hpp>

template <typename T>
void test()
{
    const T value = -125.125;

    char buffer[64];
    const auto r = boost::charconv::to_chars(buffer, buffer + sizeof(buffer), value, boost::charconv::chars_format::fixed, 0);
    BOOST_TEST(r);
    *r.ptr = '\0';

    BOOST_TEST_CSTR_EQ(buffer, "-125");
}

template <typename T>
void test_pos()
{
    const T value = 125.125;

    char buffer[64];
    const auto r = boost::charconv::to_chars(buffer, buffer + sizeof(buffer), value, boost::charconv::chars_format::fixed, 0);
    BOOST_TEST(r);
    *r.ptr = '\0';

    BOOST_TEST_CSTR_EQ(buffer, "125");
}

template <typename T>
void test_prec_1()
{
    const T value = -125.125;

    char buffer[64];
    const auto r = boost::charconv::to_chars(buffer, buffer + sizeof(buffer), value, boost::charconv::chars_format::fixed, 1);
    BOOST_TEST(r);
    *r.ptr = '\0';

    BOOST_TEST_CSTR_EQ(buffer, "-125.1");
}

int main()
{
    test<float>();
    test<double>();

    test_pos<float>();
    test_pos<double>();

    test_prec_1<float>();
    test_prec_1<double>();

    #if !defined(BOOST_CHARCONV_UNSUPPORTED_LONG_DOUBLE) && BOOST_CHARCONV_LDBL_BITS < 128
    test<long double>();
    test_pos<long double>();
    test_prec_1<long double>();
    #endif

    return boost::report_errors();
}
