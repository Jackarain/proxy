// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/charconv/detail/parser.hpp>
#include <boost/charconv/chars_format.hpp>
#include <boost/charconv/to_chars.hpp>
#include <boost/core/lightweight_test.hpp>
#include <system_error>
#include <type_traits>
#include <cstdint>
#include <cstring>
#include <cerrno>

void test_integer()
{
    std::uint64_t significand {};
    std::int64_t  exponent {};
    bool sign {};

    const char* val1 = "12";
    auto r1 = boost::charconv::detail::parser(val1, val1 + std::strlen(val1), sign, significand, exponent);
    BOOST_TEST(r1.ec == std::errc());
    BOOST_TEST_EQ(sign, false);
    BOOST_TEST_EQ(significand, UINT64_C(12));
    BOOST_TEST_EQ(exponent, 0);

    significand = 0;
    exponent = 0;
    sign = false;

    const char* val2 = "123456789";
    auto r2 = boost::charconv::detail::parser(val2, val2 + std::strlen(val2), sign, significand, exponent);
    BOOST_TEST(r2.ec == std::errc());
    BOOST_TEST_EQ(sign, false);
    BOOST_TEST_EQ(exponent, 0);
    BOOST_TEST_EQ(significand, UINT64_C(123456789));

    auto r3 = boost::charconv::detail::parser(val2, val2 + std::strlen(val2), sign, significand, exponent, boost::charconv::chars_format::scientific);
    BOOST_TEST(r3.ec == std::errc::invalid_argument);

    // Get the maximum value of the significant type
    constexpr auto max_sig_v = (std::numeric_limits<decltype(significand)>::max)();
    char max_sig_buf[boost::charconv::limits<decltype(significand)>::max_chars10 + 1u];
    const auto r4 = boost::charconv::to_chars(max_sig_buf + 1, max_sig_buf + sizeof(max_sig_buf), max_sig_v);
    if (BOOST_TEST(r4)) {

        significand = 0;
        exponent = 1;
        sign = true;

        auto r5 = boost::charconv::detail::parser(max_sig_buf + 1, r4.ptr, sign, significand, exponent);
        BOOST_TEST(r5);
        BOOST_TEST_EQ(sign, false);
        BOOST_TEST_EQ(exponent, 0);
        BOOST_TEST_EQ(significand, max_sig_v);

        significand = 0;
        exponent = 1;
        sign = false;
        max_sig_buf[0] = '-';
        auto r6 = boost::charconv::detail::parser(max_sig_buf, r4.ptr, sign, significand, exponent);
        BOOST_TEST(r6);
        BOOST_TEST_EQ(sign, true);
        BOOST_TEST_EQ(exponent, 0);
        BOOST_TEST_EQ(significand, max_sig_v);
    }

    // Small significant type
    std::uint8_t significand_8{};
    exponent = 0;
    sign = false;

    const char* val3 = "255";
    auto r5 = boost::charconv::detail::parser(val3, val3 + std::strlen(val3), sign, significand_8, exponent);
    BOOST_TEST(r5);
    BOOST_TEST_EQ(sign, false);
    BOOST_TEST_EQ(exponent, 0);
    BOOST_TEST_EQ(significand_8, 255);

    const char* val4 = "256";
    auto r6 = boost::charconv::detail::parser(val4, val4 + std::strlen(val4), sign, significand_8, exponent);
    BOOST_TEST(r6.ec == std::errc::result_out_of_range);
}

void test_scientific()
{
    std::uint64_t significand {};
    std::int64_t  exponent {};
    bool sign {};

    const char* val1 = "-1e1";
    auto r1 = boost::charconv::detail::parser(val1, val1 + std::strlen(val1), sign, significand, exponent);
    BOOST_TEST(r1.ec == std::errc());
    BOOST_TEST_EQ(sign, true);
    BOOST_TEST_EQ(significand, UINT64_C(1));
    BOOST_TEST_EQ(exponent, 1);

    significand = 0;
    exponent = 0;
    sign = false;

    const char* val2 = "123456789e10";
    auto r2 = boost::charconv::detail::parser(val2, val2 + std::strlen(val2), sign, significand, exponent);
    BOOST_TEST(r2.ec == std::errc());
    BOOST_TEST_EQ(sign, false);
    BOOST_TEST_EQ(exponent, 10);
    BOOST_TEST_EQ(significand, UINT64_C(123456789));

    significand = 0;
    exponent = 0;
    sign = false;

    const char* val3 = "1.23456789e+10";
    auto r3 = boost::charconv::detail::parser(val3, val3 + std::strlen(val3), sign, significand, exponent);
    BOOST_TEST(r3.ec == std::errc());
    BOOST_TEST_EQ(sign, false);
    BOOST_TEST_EQ(exponent, 2);
    BOOST_TEST_EQ(significand, UINT64_C(123456789));

    const char* val4 = "1.23456789e-10";
    auto r4 = boost::charconv::detail::parser(val4, val4 + std::strlen(val4), sign, significand, exponent);
    BOOST_TEST(r4.ec == std::errc());
    BOOST_TEST_EQ(sign, false);
    BOOST_TEST_EQ(exponent, -18);
    BOOST_TEST_EQ(significand, UINT64_C(123456789));

    auto r5 = boost::charconv::detail::parser(val4, val4 + std::strlen(val4), sign, significand, exponent, boost::charconv::chars_format::fixed);
    BOOST_TEST(r5.ec == std::errc::invalid_argument);

    const char* val6 = "987654321e10";
    auto r6 = boost::charconv::detail::parser(val6, val6 + std::strlen(val6), sign, significand, exponent);
    BOOST_TEST(r6.ec == std::errc());
    BOOST_TEST_EQ(sign, false);
    BOOST_TEST_EQ(exponent, 10);
    BOOST_TEST_EQ(significand, UINT64_C(987654321));

    const char* val7 = "1.23456789E+10";
    auto r7 = boost::charconv::detail::parser(val7, val7 + std::strlen(val7), sign, significand, exponent);
    BOOST_TEST(r7.ec == std::errc());
    BOOST_TEST_EQ(sign, false);
    BOOST_TEST_EQ(exponent, 2);
    BOOST_TEST_EQ(significand, UINT64_C(123456789));

}

void test_hex_integer()
{
    std::uint64_t significand {};
    std::int64_t  exponent {};
    bool sign {};

    const char* val1 = "2a";
    auto r1 = boost::charconv::detail::parser(val1, val1 + std::strlen(val1), sign, significand, exponent, boost::charconv::chars_format::hex);
    BOOST_TEST(r1.ec == std::errc());
    BOOST_TEST_EQ(sign, false);
    BOOST_TEST_EQ(significand, UINT64_C(42));
    BOOST_TEST_EQ(exponent, 0);

    significand = 0;
    exponent = 0;
    sign = false;

    const char* val2 = "-1a3b5c7d9";
    auto r2 = boost::charconv::detail::parser(val2, val2 + std::strlen(val2), sign, significand, exponent, boost::charconv::chars_format::hex);
    BOOST_TEST(r2.ec == std::errc());
    BOOST_TEST_EQ(sign, true);
    BOOST_TEST_EQ(exponent, 0);
    BOOST_TEST_EQ(significand, UINT64_C(7041566681));

    auto r3 = boost::charconv::detail::parser(val2, val2 + std::strlen(val2), sign, significand, exponent, boost::charconv::chars_format::scientific);
    BOOST_TEST(r3.ec == std::errc::invalid_argument);
}

void test_hex_scientific()
{
    std::uint64_t significand {};
    std::int64_t  exponent {};
    bool sign {};

    const char* val1 = "2ap+5";
    auto r1 = boost::charconv::detail::parser(val1, val1 + std::strlen(val1), sign, significand, exponent, boost::charconv::chars_format::hex);
    BOOST_TEST(r1.ec == std::errc());
    BOOST_TEST_EQ(sign, false);
    BOOST_TEST_EQ(significand, UINT64_C(42));
    BOOST_TEST_EQ(exponent, 5);

    significand = 0;
    exponent = 0;
    sign = false;

    const char* val2 = "-1.3a2bp-10";
    auto r2 = boost::charconv::detail::parser(val2, val2 + std::strlen(val2), sign, significand, exponent, boost::charconv::chars_format::hex);
    BOOST_TEST(r2.ec == std::errc());
    BOOST_TEST_EQ(sign, true);
    BOOST_TEST_EQ(exponent, -14);
    BOOST_TEST_EQ(significand, UINT64_C(80427));

    auto r3 = boost::charconv::detail::parser(val2, val2 + std::strlen(val2), sign, significand, exponent, boost::charconv::chars_format::scientific);
    BOOST_TEST(r3.ec == std::errc::invalid_argument);

    const char* val4 = "-1.3A2BP-10";
    auto r4 = boost::charconv::detail::parser(val4, val4 + std::strlen(val4), sign, significand, exponent, boost::charconv::chars_format::hex);
    BOOST_TEST(r4.ec == std::errc());
    BOOST_TEST_EQ(sign, true);
    BOOST_TEST_EQ(exponent, -14);
    BOOST_TEST_EQ(significand, UINT64_C(80427));
}

void test_zeroes()
{
    for (const char* val : {
        "0", "00", "000", "0000",
        "0.", "00.", "000.", "0000.",
        "0.0", "00.0", "000.0", "0000.0",
        "0e0", "00e0", "000e0", "0000e0",
        "0.e0", "00.e0", "000.e0", "0000.e0",
        "0.0e0", "00.0e0", "000.0e0", "0000.0e0",
        }) {
        // Use small integer type to reduce input test string lengths
        std::uint8_t significand = 1;
        std::int64_t exponent = 1;
        bool sign = (std::strlen(val) % 2) == 0; // Different initial values

        auto r1 = boost::charconv::detail::parser(val, val + std::strlen(val), sign, significand, exponent);
        BOOST_TEST(r1);
        BOOST_TEST_EQ(sign, false);
        BOOST_TEST_EQ(significand, 0u);
        BOOST_TEST_EQ(exponent, 0);
    }
}

int main()
{
    test_integer();
    test_scientific();
    test_hex_integer();
    test_hex_scientific();

    test_zeroes();
    return boost::report_errors();
}
