// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/decimal.hpp>
#include <boost/core/lightweight_test.hpp>
#include <limits>
#include <array>
#include <cctype>
#include <string>

using namespace boost::decimal;

#ifdef BOOST_DECIMAL_HAS_FORMAT_SUPPORT

template <typename T>
void test_general()
{
    BOOST_TEST_EQ(std::format("{}", T{1}), "1");
    BOOST_TEST_EQ(std::format("{}", T{10}), "10");
    BOOST_TEST_EQ(std::format("{}", T{100}), "100");
    BOOST_TEST_EQ(std::format("{}", T{1000}), "1000");
    BOOST_TEST_EQ(std::format("{}", T{10000}), "10000");
    BOOST_TEST_EQ(std::format("{}", T{210000}), "210000");
    BOOST_TEST_EQ(std::format("{}", T{2100000}), "2100000");

    if constexpr (std::numeric_limits<T>::digits10 <= 7)
    {
        BOOST_TEST_EQ(std::format("{}", T {21, 6}), "2.1e+07");
        BOOST_TEST_EQ(std::format("{}", T {211, 6}), "2.11e+08");
        BOOST_TEST_EQ(std::format("{}", T {2111, 6}), "2.111e+09");
    }
    else
    {
        BOOST_TEST_EQ(std::format("{}", T {21, 6}), "21000000");
        BOOST_TEST_EQ(std::format("{}", T {211, 6}), "211000000");
        BOOST_TEST_EQ(std::format("{}", T {2111, 6}), "2111000000");
    }

    BOOST_TEST_EQ(std::format("{}", T{}), "0");
    BOOST_TEST_EQ(std::format("{}", std::numeric_limits<T>::infinity()), "inf");
    BOOST_TEST_EQ(std::format("{}", -std::numeric_limits<T>::infinity()), "-inf");
    BOOST_TEST_EQ(std::format("{}", std::numeric_limits<T>::quiet_NaN()), "nan");
    BOOST_TEST_EQ(std::format("{}", -std::numeric_limits<T>::quiet_NaN()), "-nan(ind)");
    BOOST_TEST_EQ(std::format("{}", std::numeric_limits<T>::signaling_NaN()), "nan(snan)");
    BOOST_TEST_EQ(std::format("{}", -std::numeric_limits<T>::signaling_NaN()), "-nan(snan)");

    BOOST_TEST_EQ(std::format("{:g}", T{1}), "1");
    BOOST_TEST_EQ(std::format("{:g}", T{10}), "10");
    BOOST_TEST_EQ(std::format("{:g}", T{100}), "100");
    BOOST_TEST_EQ(std::format("{:g}", T{1000}), "1000");
    BOOST_TEST_EQ(std::format("{:g}", T{10000}), "10000");
    BOOST_TEST_EQ(std::format("{:g}", T{210000}), "210000");
    BOOST_TEST_EQ(std::format("{:g}", T{2100000}), "2100000");

    if constexpr (std::numeric_limits<T>::digits10 <= 7)
    {
        BOOST_TEST_EQ(std::format("{:g}", T {-21, 6}), "-2.1e+07");
        BOOST_TEST_EQ(std::format("{:g}", T {-211, 6}), "-2.11e+08");
        BOOST_TEST_EQ(std::format("{:g}", T {-2111, 6}), "-2.111e+09");
        BOOST_TEST_EQ(std::format("{:G}", T {-21, 6}), "-2.1E+07");
        BOOST_TEST_EQ(std::format("{:G}", T {-211, 6}), "-2.11E+08");
        BOOST_TEST_EQ(std::format("{:G}", T {-2111, 6}), "-2.111E+09");
    }
    else
    {
        BOOST_TEST_EQ(std::format("{:g}", T {-21, 6}), "-21000000");
        BOOST_TEST_EQ(std::format("{:g}", T {-211, 6}), "-211000000");
        BOOST_TEST_EQ(std::format("{:g}", T {-2111, 6}), "-2111000000");
    }

    BOOST_TEST_EQ(std::format("{:g}", std::numeric_limits<T>::infinity()), "inf");
    BOOST_TEST_EQ(std::format("{:g}", -std::numeric_limits<T>::infinity()), "-inf");
    BOOST_TEST_EQ(std::format("{:g}", std::numeric_limits<T>::quiet_NaN()), "nan");
    BOOST_TEST_EQ(std::format("{:g}", -std::numeric_limits<T>::quiet_NaN()), "-nan(ind)");
    BOOST_TEST_EQ(std::format("{:g}", std::numeric_limits<T>::signaling_NaN()), "nan(snan)");
    BOOST_TEST_EQ(std::format("{:g}", -std::numeric_limits<T>::signaling_NaN()), "-nan(snan)");

    BOOST_TEST_EQ(std::format("{:G}", std::numeric_limits<T>::infinity()), "INF");
    BOOST_TEST_EQ(std::format("{:G}", -std::numeric_limits<T>::infinity()), "-INF");
    BOOST_TEST_EQ(std::format("{:G}", std::numeric_limits<T>::quiet_NaN()), "NAN");
    BOOST_TEST_EQ(std::format("{:G}", -std::numeric_limits<T>::quiet_NaN()), "-NAN(IND)");
    BOOST_TEST_EQ(std::format("{:G}", std::numeric_limits<T>::signaling_NaN()), "NAN(SNAN)");
    BOOST_TEST_EQ(std::format("{:G}", -std::numeric_limits<T>::signaling_NaN()), "-NAN(SNAN)");
}

template <typename T>
void test_fixed()
{
    BOOST_TEST_EQ(std::format("{:f}", T {-21, 6}), "-21000000");
    BOOST_TEST_EQ(std::format("{:f}", T {-211, 6}), "-211000000");
    BOOST_TEST_EQ(std::format("{:f}", T {-2111, 6}), "-2111000000");

    BOOST_TEST_EQ(std::format("{:.0f}", T {-21, 6}), std::string{"-21000000"});
    BOOST_TEST_EQ(std::format("{:.0f}", T {-211, 6}), std::string{"-211000000"});
    BOOST_TEST_EQ(std::format("{:.0f}", T {-2111, 6}), std::string{"-2111000000"});

    BOOST_TEST_EQ(std::format("{:.1f}", T {-21, 6}), std::string{"-21000000.0"});
    BOOST_TEST_EQ(std::format("{:.1f}", T {-211, 6}), std::string{"-211000000.0"});
    BOOST_TEST_EQ(std::format("{:.1f}", T {-2111, 6}), std::string{"-2111000000.0"});

    BOOST_TEST_EQ(std::format("{:.0f}", T {0}), "0");
    BOOST_TEST_EQ(std::format("{:f}", std::numeric_limits<T>::infinity()), "inf");
    BOOST_TEST_EQ(std::format("{:f}", -std::numeric_limits<T>::infinity()), "-inf");
    BOOST_TEST_EQ(std::format("{:f}", std::numeric_limits<T>::quiet_NaN()), "nan");
    BOOST_TEST_EQ(std::format("{:f}", -std::numeric_limits<T>::quiet_NaN()), "-nan(ind)");
    BOOST_TEST_EQ(std::format("{:f}", std::numeric_limits<T>::signaling_NaN()), "nan(snan)");
    BOOST_TEST_EQ(std::format("{:f}", -std::numeric_limits<T>::signaling_NaN()), "-nan(snan)");

    BOOST_TEST_EQ(std::format("{:F}", std::numeric_limits<T>::infinity()), "INF");
    BOOST_TEST_EQ(std::format("{:F}", -std::numeric_limits<T>::infinity()), "-INF");
    BOOST_TEST_EQ(std::format("{:F}", std::numeric_limits<T>::quiet_NaN()), "NAN");
    BOOST_TEST_EQ(std::format("{:F}", -std::numeric_limits<T>::quiet_NaN()), "-NAN(IND)");
    BOOST_TEST_EQ(std::format("{:F}", std::numeric_limits<T>::signaling_NaN()), "NAN(SNAN)");
    BOOST_TEST_EQ(std::format("{:F}", -std::numeric_limits<T>::signaling_NaN()), "-NAN(SNAN)");
}

template <typename T>
void test_scientific()
{
    BOOST_TEST_EQ(std::format("{:e}", T {-21, 6}), "-2.1e+07");
    BOOST_TEST_EQ(std::format("{:e}", T {-211, 6}), "-2.11e+08");
    BOOST_TEST_EQ(std::format("{:e}", T {-2111, 6}), "-2.111e+09");

    BOOST_TEST_EQ(std::format("{:E}", T {-21, 6}), "-2.1E+07");
    BOOST_TEST_EQ(std::format("{:E}", T {-211, 6}), "-2.11E+08");
    BOOST_TEST_EQ(std::format("{:E}", T {-2111, 6}), "-2.111E+09");

    BOOST_TEST_EQ(std::format("{:.0E}", T {0}), "0E+00");
    BOOST_TEST_EQ(std::format("{:e}", std::numeric_limits<T>::infinity()), "inf");
    BOOST_TEST_EQ(std::format("{:e}", -std::numeric_limits<T>::infinity()), "-inf");
    BOOST_TEST_EQ(std::format("{:e}", std::numeric_limits<T>::quiet_NaN()), "nan");
    BOOST_TEST_EQ(std::format("{:e}", -std::numeric_limits<T>::quiet_NaN()), "-nan(ind)");
    BOOST_TEST_EQ(std::format("{:e}", std::numeric_limits<T>::signaling_NaN()), "nan(snan)");
    BOOST_TEST_EQ(std::format("{:e}", -std::numeric_limits<T>::signaling_NaN()), "-nan(snan)");

    BOOST_TEST_EQ(std::format("{:-e}", std::numeric_limits<T>::infinity()), "inf");
    BOOST_TEST_EQ(std::format("{:-e}", -std::numeric_limits<T>::infinity()), "-inf");
    BOOST_TEST_EQ(std::format("{:-e}", std::numeric_limits<T>::quiet_NaN()), "nan");
    BOOST_TEST_EQ(std::format("{:-e}", -std::numeric_limits<T>::quiet_NaN()), "-nan(ind)");
    BOOST_TEST_EQ(std::format("{:-e}", std::numeric_limits<T>::signaling_NaN()), "nan(snan)");
    BOOST_TEST_EQ(std::format("{:-e}", -std::numeric_limits<T>::signaling_NaN()), "-nan(snan)");

    BOOST_TEST_EQ(std::format("{:+e}", std::numeric_limits<T>::infinity()), "+inf");
    BOOST_TEST_EQ(std::format("{:+e}", -std::numeric_limits<T>::infinity()), "-inf");
    BOOST_TEST_EQ(std::format("{:+e}", std::numeric_limits<T>::quiet_NaN()), "+nan");
    BOOST_TEST_EQ(std::format("{:+e}", -std::numeric_limits<T>::quiet_NaN()), "-nan(ind)");
    BOOST_TEST_EQ(std::format("{:+e}", std::numeric_limits<T>::signaling_NaN()), "+nan(snan)");
    BOOST_TEST_EQ(std::format("{:+e}", -std::numeric_limits<T>::signaling_NaN()), "-nan(snan)");

    BOOST_TEST_EQ(std::format("{: e}", std::numeric_limits<T>::infinity()), " inf");
    BOOST_TEST_EQ(std::format("{: e}", -std::numeric_limits<T>::infinity()), "-inf");
    BOOST_TEST_EQ(std::format("{: e}", std::numeric_limits<T>::quiet_NaN()), " nan");
    BOOST_TEST_EQ(std::format("{: e}", -std::numeric_limits<T>::quiet_NaN()), "-nan(ind)");
    BOOST_TEST_EQ(std::format("{: e}", std::numeric_limits<T>::signaling_NaN()), " nan(snan)");
    BOOST_TEST_EQ(std::format("{: e}", -std::numeric_limits<T>::signaling_NaN()), "-nan(snan)");

    BOOST_TEST_EQ(std::format("{:E}", std::numeric_limits<T>::infinity()), "INF");
    BOOST_TEST_EQ(std::format("{:E}", -std::numeric_limits<T>::infinity()), "-INF");
    BOOST_TEST_EQ(std::format("{:E}", std::numeric_limits<T>::quiet_NaN()), "NAN");
    BOOST_TEST_EQ(std::format("{:E}", -std::numeric_limits<T>::quiet_NaN()), "-NAN(IND)");
    BOOST_TEST_EQ(std::format("{:E}", std::numeric_limits<T>::signaling_NaN()), "NAN(SNAN)");
    BOOST_TEST_EQ(std::format("{:E}", -std::numeric_limits<T>::signaling_NaN()), "-NAN(SNAN)");

    // Width with right-align
    // "0.0E+00" is 7 chars, width 10 means 3 spaces padding
    BOOST_TEST_EQ(std::format("{:>10.1E}", T {0}), "   0.0E+00");
    // "0.000E+00" is 9 chars, width 10 means 1 space padding
    BOOST_TEST_EQ(std::format("{:>10.3E}", T {0}), " 0.000E+00");

    // With + sign: "+0.0E+00" is 8 chars, width 10 means 2 spaces padding
    BOOST_TEST_EQ(std::format("{:>+10.1E}", T {0}), "  +0.0E+00");
    // "+0.000E+00" is 10 chars, no padding needed
    BOOST_TEST_EQ(std::format("{:>+10.3E}", T {0}), "+0.000E+00");

    // With space sign: " 0.0E+00" is 8 chars, width 10 means 2 spaces padding
    BOOST_TEST_EQ(std::format("{:> 10.1E}", T {0}), "   0.0E+00");
    // " 0.000E+00" is 10 chars, no padding needed
    BOOST_TEST_EQ(std::format("{:> 10.3E}", T {0}), " 0.000E+00");

    // Zero-fill with explicit 0 fill character
    BOOST_TEST_EQ(std::format("{:0>10.1E}", T {0}), "0000.0E+00");
    BOOST_TEST_EQ(std::format("{:0>10.3E}", T {0}), "00.000E+00");
}

template <typename T>
void test_hex()
{
    BOOST_TEST_EQ(std::format("{:.0x}", T {0}), "0p+00");
    BOOST_TEST_EQ(std::format("{:.3X}", T {0}), "0.000P+00");
    BOOST_TEST_EQ(std::format("{:x}", std::numeric_limits<T>::infinity()), "inf");
    BOOST_TEST_EQ(std::format("{:x}", -std::numeric_limits<T>::infinity()), "-inf");
    BOOST_TEST_EQ(std::format("{:x}", std::numeric_limits<T>::quiet_NaN()), "nan");
    BOOST_TEST_EQ(std::format("{:x}", -std::numeric_limits<T>::quiet_NaN()), "-nan(ind)");
    BOOST_TEST_EQ(std::format("{:x}", std::numeric_limits<T>::signaling_NaN()), "nan(snan)");
    BOOST_TEST_EQ(std::format("{:x}", -std::numeric_limits<T>::signaling_NaN()), "-nan(snan)");

    BOOST_TEST_EQ(std::format("{:X}", std::numeric_limits<T>::infinity()), "INF");
    BOOST_TEST_EQ(std::format("{:X}", -std::numeric_limits<T>::infinity()), "-INF");
    BOOST_TEST_EQ(std::format("{:X}", std::numeric_limits<T>::quiet_NaN()), "NAN");
    BOOST_TEST_EQ(std::format("{:X}", -std::numeric_limits<T>::quiet_NaN()), "-NAN(IND)");
    BOOST_TEST_EQ(std::format("{:X}", std::numeric_limits<T>::signaling_NaN()), "NAN(SNAN)");
    BOOST_TEST_EQ(std::format("{:X}", -std::numeric_limits<T>::signaling_NaN()), "-NAN(SNAN)");
}

template <typename T>
void test_with_string()
{
    BOOST_TEST_EQ(std::format("Height is: {:.0f} meters", T {0}), "Height is: 0 meters");
    BOOST_TEST_EQ(std::format("Height is: {} meters", T {2}), "Height is: 2 meters");
}

template <typename T>
void test_cohort_preservation()
{
    const std::array<T, 7> decimals = {
        T {5, 4},
        T {50, 3},
        T {500, 2},
        T {5000, 1},
        T {50000, 0},
        T {500000, -1},
        T {5000000, -2},
    };

    const std::array<const char*, 7> result_strings = {
        "5e+04",
        "5.0e+04",
        "5.00e+04",
        "5.000e+04",
        "5.0000e+04",
        "5.00000e+04",
        "5.000000e+04",
    };

    for (std::size_t i {}; i < decimals.size(); ++i)
    {
        BOOST_TEST_CSTR_EQ(std::format("{:a}", decimals[i]).c_str(), result_strings[i]);

        std::string s {result_strings[i]};

        #ifdef _MSC_VER
        #  pragma warning(push)
        #  pragma warning(disable : 4244)
        #endif

        std::transform(s.begin(), s.end(), s.begin(),
                           [](unsigned char c)
                           { return std::toupper(c); });

        #ifdef _MSC_VER
        #  pragma warning(pop)
        #endif

        BOOST_TEST_CSTR_EQ(std::format("{:A}", decimals[i]).c_str(), s.c_str());
    }
}

template <typename T>
void test_locale_conversion(const char* locale, const std::string& result)
{
    try
    {
        const std::locale a(locale);
        std::locale::global(a);

        const T value {112289, -2};
        BOOST_TEST_EQ(std::format("{:.2fL}", value), result);
    }
    // LCOV_EXCL_START
    catch (...)
    {
        std::cerr << "Test not run" << std::endl;
    }
    // LCOV_EXCL_STOP
}

template <typename T>
void test_wide_strings()
{
    BOOST_TEST(std::format(L"{}", T{1}) == L"1");
    BOOST_TEST(std::format(L"{}", T{10}) == L"10");
    BOOST_TEST(std::format(L"{}", T{100}) == L"100");
    BOOST_TEST(std::format(L"{}", T{1000}) == L"1000");
    BOOST_TEST(std::format(L"{}", T{10000}) == L"10000");
    BOOST_TEST(std::format(L"{}", T{210000}) == L"210000");
    BOOST_TEST(std::format(L"{}", T{2100000}) == L"2100000");
}

template <typename T>
void test_alignment_and_fill()
{
    // Right alignment
    BOOST_TEST_EQ(std::format("{:>10}", T{42}), "        42");
    BOOST_TEST_EQ(std::format("{:>10f}", T{42}), "        42");
    BOOST_TEST_EQ(std::format("{:>10.2f}", T{3u, -2}), "      0.03");

    // Left alignment
    BOOST_TEST_EQ(std::format("{:<10}", T{42}), "42        ");
    BOOST_TEST_EQ(std::format("{:<10f}", T{42}), "42        ");
    BOOST_TEST_EQ(std::format("{:<10.2f}", T{3u, -2}), "0.03      ");

    // Center alignment
    BOOST_TEST_EQ(std::format("{:^10}", T{42}), "    42    ");
    BOOST_TEST_EQ(std::format("{:^10f}", T{42}), "    42    ");
    BOOST_TEST_EQ(std::format("{:^11}", T{42}), "    42     ");  // Odd width, extra space on right

    // Custom fill character with right alignment
    BOOST_TEST_EQ(std::format("{:*>10}", T{42}), "********42");
    BOOST_TEST_EQ(std::format("{:->10}", T{42}), "--------42");
    BOOST_TEST_EQ(std::format("{:0>10}", T{42}), "0000000042");

    // Custom fill character with left alignment
    BOOST_TEST_EQ(std::format("{:*<10}", T{42}), "42********");
    BOOST_TEST_EQ(std::format("{:-<10}", T{42}), "42--------");

    // Custom fill character with center alignment
    BOOST_TEST_EQ(std::format("{:*^10}", T{42}), "****42****");
    BOOST_TEST_EQ(std::format("{:-^10}", T{42}), "----42----");
    BOOST_TEST_EQ(std::format("{:=^11}", T{42}), "====42=====");

    // Alignment with negative values
    BOOST_TEST_EQ(std::format("{:>10}", -T{42}), "       -42");
    BOOST_TEST_EQ(std::format("{:<10}", -T{42}), "-42       ");
    BOOST_TEST_EQ(std::format("{:^10}", -T{42}), "   -42    ");

    // Alignment with explicit plus sign
    BOOST_TEST_EQ(std::format("{:>+10}", T{42}), "       +42");
    BOOST_TEST_EQ(std::format("{:<+10}", T{42}), "+42       ");
    BOOST_TEST_EQ(std::format("{:^+10}", T{42}), "   +42    ");

    // Fill and alignment with precision
    BOOST_TEST_EQ(std::format("{:*>12.2e}", T{314159u, -5}), "****3.14e+00");
    BOOST_TEST_EQ(std::format("{:*<12.2e}", T{314159u, -5}), "3.14e+00****");
    BOOST_TEST_EQ(std::format("{:*^12.2e}", T{314159u, -5}), "**3.14e+00**");

    // Width smaller than content (should not truncate)
    BOOST_TEST_EQ(std::format("{:>2}", T{12345}), "12345");
    BOOST_TEST_EQ(std::format("{:<2}", T{12345}), "12345");
    BOOST_TEST_EQ(std::format("{:^2}", T{12345}), "12345");

    // Special values with alignment
    BOOST_TEST_EQ(std::format("{:>10}", std::numeric_limits<T>::infinity()), "       inf");
    BOOST_TEST_EQ(std::format("{:<10}", std::numeric_limits<T>::infinity()), "inf       ");
    BOOST_TEST_EQ(std::format("{:^10}", std::numeric_limits<T>::infinity()), "   inf    ");
    BOOST_TEST_EQ(std::format("{:*>10}", std::numeric_limits<T>::quiet_NaN()), "*******nan");

    // Default alignment (none specified) with width - per std::format spec, right-align with space fill
    BOOST_TEST_EQ(std::format("{:10}", T{42}), "        42");
}

int main()
{
    test_general<decimal32_t>();
    test_general<decimal_fast32_t>();
    test_general<decimal64_t>();
    test_general<decimal_fast64_t>();
    test_general<decimal128_t>();
    test_general<decimal_fast128_t>();

    test_fixed<decimal32_t>();
    test_fixed<decimal_fast32_t>();
    test_fixed<decimal64_t>();
    test_fixed<decimal_fast64_t>();
    test_fixed<decimal128_t>();
    test_fixed<decimal_fast128_t>();

    test_scientific<decimal32_t>();
    test_scientific<decimal_fast32_t>();
    test_scientific<decimal64_t>();
    test_scientific<decimal_fast64_t>();
    test_scientific<decimal128_t>();
    test_scientific<decimal_fast128_t>();

    test_hex<decimal32_t>();
    test_hex<decimal_fast32_t>();
    test_hex<decimal64_t>();
    test_hex<decimal_fast64_t>();
    test_hex<decimal128_t>();
    test_hex<decimal_fast128_t>();

    test_with_string<decimal32_t>();
    test_with_string<decimal_fast32_t>();
    test_with_string<decimal64_t>();
    test_with_string<decimal_fast64_t>();
    test_with_string<decimal128_t>();
    test_with_string<decimal_fast128_t>();

    test_cohort_preservation<decimal32_t>();
    test_cohort_preservation<decimal64_t>();
    test_cohort_preservation<decimal128_t>();

    #if !(defined(__GNUC__) && __GNUC__ >= 5 && defined(__APPLE__)) && !defined(BOOST_DECIMAL_QEMU_TEST) && !defined(BOOST_DECIMAL_DISABLE_EXCEPTIONS) && !defined(_MSC_VER)

    test_locale_conversion<decimal32_t>("en_US.UTF-8", "1,122.89");
    test_locale_conversion<decimal32_t>("de_DE.UTF-8", "1.122,89");

    #if (defined(__clang__) && __clang_major__ > 9) || (defined(__GNUC__) && __GNUC__ > 9)
    test_locale_conversion<decimal32_t>("fr_FR.UTF-8", "1 122,89");
    #endif

    #endif

    test_wide_strings<decimal32_t>();
    test_wide_strings<decimal_fast32_t>();
    test_wide_strings<decimal64_t>();
    test_wide_strings<decimal_fast64_t>();
    test_wide_strings<decimal128_t>();
    test_wide_strings<decimal_fast128_t>();

    test_alignment_and_fill<decimal32_t>();
    test_alignment_and_fill<decimal_fast32_t>();
    test_alignment_and_fill<decimal64_t>();
    test_alignment_and_fill<decimal_fast64_t>();
    test_alignment_and_fill<decimal128_t>();
    test_alignment_and_fill<decimal_fast128_t>();

    return boost::report_errors();
}

#else

int main()
{
    return 0;
}

#endif
