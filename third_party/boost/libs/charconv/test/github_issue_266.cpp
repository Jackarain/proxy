// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// See: https://github.com/boostorg/charconv/issues/266

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
    test(123456789012345.6789012345678901, 5, "123456789012345.67188");
    test(1234567890123456.789012345678901, 5, "1234567890123456.75000");
    test(12345678901234567.89012345678901, 5, "12345678901234568.00000");
    test(123456789012345678.9012345678901, 5, "123456789012345680.00000");
    test(1234567890123456789.012345678901, 5, "1234567890123456768.00000");

    test(123456789012345678901234567.8901, 5, "123456789012345678152597504.00000");
    test(12345678901234567890123456.78901, 5, "12345678901234568244756480.00000");
    test(1234567890123456789012345.678901, 5, "1234567890123456824475648.00000");
    test(123456789012345678901234.5678901, 5, "123456789012345685803008.00000");
    test(12345678901234567890123.45678901, 5, "12345678901234567741440.00000");
    test(123456789012345678901.2345678901, 5, "123456789012345683968.00000");

    return boost::report_errors();
}
