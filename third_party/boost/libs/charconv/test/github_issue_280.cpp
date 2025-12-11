// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// See: https://github.com/boostorg/charconv/issues/280

#include <boost/charconv.hpp>
#include <boost/core/lightweight_test.hpp>
#include <string>

template <std::size_t precision>
void test(const char* result_string)
{
    constexpr double d = DBL_MIN;
    // for scientific, precision + 7 should suffice:
    // <digit> <.> <#precision digits> <e> <sign> <up to 3 digits>
    //  1       2                       3   4      7
    constexpr size_t buf_size = precision + 7;
    char buf[buf_size];

    auto result = boost::charconv::to_chars(buf, buf + buf_size, d, boost::charconv::chars_format::scientific, precision);
    BOOST_TEST(result);

    // We have to use memcmp here since there is no space to null terminate
    BOOST_TEST(std::memcmp(buf, result_string, precision) == 0);
}

template <std::size_t precision>
void test_negative(const char* result_string)
{
    constexpr double d = -DBL_MIN;
    // for scientific, precision + 7 should suffice:
    // <digit> <.> <#precision digits> <e> <sign> <up to 3 digits>
    //  1       2                       3   4      7
    constexpr size_t buf_size = 1 + precision + 7;
    char buf[buf_size];

    auto result = boost::charconv::to_chars(buf, buf + buf_size, d, boost::charconv::chars_format::scientific, precision);
    BOOST_TEST(result);

    // We have to use memcmp here since there is no space to null terminate
    BOOST_TEST(std::memcmp(buf, result_string, precision) == 0);
}

int main()
{
    // DBL_MIN = 2.2250738585072013830902327173324040642192159804623318306e-308
    test<1>("2.2e-308");
    test<2>("2.23e-308");
    test<3>("2.225e-308");
    test<4>("2.2251e-308");
    test<5>("2.22507e-308");

    test_negative<1>("-2.2e-308");
    test_negative<2>("-2.23e-308");
    test_negative<3>("-2.225e-308");
    test_negative<4>("-2.2251e-308");
    test_negative<5>("-2.22507e-308");

    return boost::report_errors();
}
