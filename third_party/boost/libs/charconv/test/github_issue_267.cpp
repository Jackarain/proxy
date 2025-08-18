// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// See: https://github.com/boostorg/charconv/issues/267

#include <boost/charconv.hpp>
#include <boost/core/lightweight_test.hpp>

template <typename T>
void test(T value, int precision, const char* correct_result)
{
    char buffer[64] {};
    const auto r = boost::charconv::to_chars(buffer, buffer + sizeof(buffer), value, boost::charconv::chars_format::fixed, precision);
    BOOST_TEST_CSTR_EQ(buffer, correct_result);
    BOOST_TEST(r);
}

int main()
{
    test(0.09, 2, "0.09");
    test(0.099, 2, "0.10");
    test(0.0999, 2, "0.10");
    test(0.09999, 2, "0.10");
    test(0.099999, 2, "0.10");
    test(0.0999999, 2, "0.10");
    test(0.09999999, 2, "0.10");
    test(0.099999999, 2, "0.10");
    test(0.0999999999, 2, "0.10");
    test(0.09999999999, 2, "0.10");
    test(0.099999999999, 2, "0.10");
    test(0.0999999999999, 2, "0.10");

    return boost::report_errors();
}
