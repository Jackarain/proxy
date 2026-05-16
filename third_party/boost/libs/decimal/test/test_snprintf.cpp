// Copyright 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/decimal/decimal32_t.hpp>
#include <boost/decimal/decimal64_t.hpp>
#include <boost/decimal/decimal128_t.hpp>
#include <boost/decimal/cstdio.hpp>
#include <iostream>
#include <iomanip>
#include <random>
#include <cmath>
#include <climits>
#include <cerrno>
#include <limits>
#include <string>
#include <algorithm>

#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wold-style-cast"
#  pragma clang diagnostic ignored "-Wundef"
#  pragma clang diagnostic ignored "-Wconversion"
#  pragma clang diagnostic ignored "-Wsign-conversion"
#  pragma clang diagnostic ignored "-Wfloat-equal"
#elif defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wold-style-cast"
#  pragma GCC diagnostic ignored "-Wundef"
#  pragma GCC diagnostic ignored "-Wconversion"
#  pragma GCC diagnostic ignored "-Wsign-conversion"
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#elif defined(_MSC_VER)
#  pragma warning(push)
#  pragma warning(disable : 4244) //Implicit conversion from char to int in toupper
#endif

#include <boost/core/lightweight_test.hpp>

using namespace boost::decimal;

template <typename T>
void test(T value, const char* format_sprintf, chars_format fmt = chars_format::general, int precision = 6)
{
    char buffer[256];
    errno = 0;

    const int num_bytes = boost::decimal::snprintf(buffer, sizeof(buffer), format_sprintf, value);

    BOOST_TEST_EQ(errno, 0);

    char charconv_buffer[256] {};
    auto first = charconv_buffer;
    auto format_first = format_sprintf;

    while (*format_first != '%')
    {
        *first++ = *format_first++;
    }

    auto r = to_chars(first, charconv_buffer + sizeof(charconv_buffer), value, fmt, precision);
    BOOST_TEST(r);
    *r.ptr = '\0';

    BOOST_TEST_CSTR_EQ(buffer, charconv_buffer);
    BOOST_TEST_EQ(num_bytes, r.ptr - charconv_buffer);
}

template <typename T>
void test_uppercase(T value, const char* format_sprintf, chars_format fmt = chars_format::general, int precision = 6)
{
    char buffer[256];
    errno = 0;

    int num_bytes = boost::decimal::snprintf(buffer, sizeof(buffer), format_sprintf, value);

    BOOST_TEST_EQ(errno, 0);

    char charconv_buffer[256];
    auto r = to_chars(charconv_buffer, charconv_buffer + sizeof(charconv_buffer), value, fmt, precision);
    BOOST_TEST(r);
    *r.ptr = '\0';

    std::string str {charconv_buffer};
    std::transform(str.begin(), str.end(), str.begin(), ::toupper);

    BOOST_TEST_CSTR_EQ(buffer, str.c_str());
    BOOST_TEST_EQ(num_bytes, r.ptr - charconv_buffer);
}

#if !(defined(__APPLE__) && defined(__GNUC__) && !defined(__clang__)) && !defined(BOOST_DECIMAL_DISABLE_EXCEPTIONS)
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

    char printf_buffer[256];
    snprintf(printf_buffer, sizeof(printf_buffer), "%.4De", decimal64_t{11897, -2});
    BOOST_TEST_CSTR_EQ(printf_buffer, buffer);
}
#endif

template <typename T>
void test_bootstrap()
{
    constexpr std::size_t N = 128;
    const char* format = std::is_same<T, decimal32_t>::value ? "%H" :
                         std::is_same<T, decimal64_t>::value ? "%D" : "%DD";

    const char* format_w_spaces = std::is_same<T, decimal32_t>::value ? "     %H" :
                                  std::is_same<T, decimal64_t>::value ? "  %D" : "\t%DD";

    const char* general_format = std::is_same<T, decimal32_t>::value ? "%Hg" :
                                 std::is_same<T, decimal64_t>::value ? "%Dg" : "%DDg";

    const char* general_upper_format = std::is_same<T, decimal32_t>::value ? "%HG" :
                                       std::is_same<T, decimal64_t>::value ? "%DG" : "%DDG";

    const char* three_digit_format = std::is_same<T, decimal32_t>::value ? "%.3H" :
                                     std::is_same<T, decimal64_t>::value ? "%.3D" : "%.3DD";

    const char* scientific_format = std::is_same<T, decimal32_t>::value ? "%He" :
                                    std::is_same<T, decimal64_t>::value ? "%De" : "%DDe";

    const char* four_digit_scientific_format = std::is_same<T, decimal32_t>::value ? "%.4He" :
                                               std::is_same<T, decimal64_t>::value ? "%.4De" : "%.4DDe";

    const char* four_digit_upper_sci_format =  std::is_same<T, decimal32_t>::value ? "%.5HE" :
                                               std::is_same<T, decimal64_t>::value ? "%.5DE" : "%.5DDE";

    const char* fixed_format = std::is_same<T, decimal32_t>::value ? "%Hf" :
                               std::is_same<T, decimal64_t>::value ? "%Df" : "%DDf";

    const char* two_digit_fixed_format = std::is_same<T, decimal32_t>::value ? "%.2Hf" :
                                         std::is_same<T, decimal64_t>::value ? "%.2Df" : "%.2DDf";

    const char* hex_format = std::is_same<T, decimal32_t>::value ? "%Ha" :
                             std::is_same<T, decimal64_t>::value ? "%Da" : "%DDa";

    const char* hex_upper_format = std::is_same<T, decimal32_t>::value ? "%HA" :
                                   std::is_same<T, decimal64_t>::value ? "%DA" : "%DDA";

    const char* one_digit_hex_upper_format = std::is_same<T, decimal32_t>::value ? "%.1HA" :
                                             std::is_same<T, decimal64_t>::value ? "%.1DA" : "%.1DDA";

    std::mt19937_64 rng(42);

    for (std::size_t i {}; i < N; ++i)
    {
        // General test
        test(T{rng()}, format);
        test(T{rng()}, format_w_spaces);
        test(T{rng()}, general_format);
        test(T{rng()}, three_digit_format, chars_format::general, 3);
        test_uppercase(T{rng()}, general_upper_format);

        // Scientific format
        test(T{rng()}, scientific_format, chars_format::scientific);
        test(T{rng()}, four_digit_scientific_format, chars_format::scientific, 4);
        test_uppercase(T{rng()}, four_digit_upper_sci_format, chars_format::scientific, 5);

        // Fixed
        test(T{rng()}, fixed_format, chars_format::fixed);
        test(T{rng()}, two_digit_fixed_format, chars_format::fixed, 2);

        // Hex
        test(T{rng()}, hex_format, chars_format::hex);
        test_uppercase(T{rng()}, hex_upper_format, chars_format::hex);
        test_uppercase(T{rng()}, one_digit_hex_upper_format, chars_format::hex, 1);
    }
}

void test_fuzzer_crash(const char* c_data)
{
    auto size = std::strlen(c_data);

    const auto formats = std::array<boost::decimal::chars_format, 4>{boost::decimal::chars_format::general,
                                                                     boost::decimal::chars_format::fixed,
                                                                     boost::decimal::chars_format::scientific,
                                                                     boost::decimal::chars_format::hex};

    const auto dec32_printf_formats = std::array<const char*, 4>{"%Hg", "%Hf", "%He", "%Ha"};
    const auto dec64_printf_formats = std::array<const char*, 4>{"%Dg", "%Df", "%De", "%Da"};
    const auto dec128_printf_formats = std::array<const char*, 4>{"%DDg", "%DDf", "%DDe", "%DDa"};

    for (std::size_t i {}; i < 4; ++i)
    {
        char buffer[20]; // Small enough it should overflow sometimes

        boost::decimal::decimal32_t f_val {};
        boost::decimal::from_chars(c_data, c_data + size, f_val, formats[i]);
        boost::decimal::snprintf(buffer, sizeof(buffer), dec32_printf_formats[i], f_val);

        boost::decimal::decimal64_t val {};
        boost::decimal::from_chars(c_data, c_data + size, val, formats[i]);
        boost::decimal::snprintf(buffer, sizeof(buffer), dec64_printf_formats[i], val);

        boost::decimal::decimal128_t ld_val {};
        boost::decimal::from_chars(c_data, c_data + size, ld_val, formats[i]);
        boost::decimal::snprintf(buffer, sizeof(buffer), dec128_printf_formats[i], ld_val);
    }
}

template <typename DecimalType>
void test_bad_input()
{
    DecimalType val {};
    char buffer[256];

    // Check for only % sign
    boost::decimal::snprintf(buffer, sizeof(buffer), "%", val);

    // Check for bad precision
    boost::decimal::snprintf(buffer, sizeof(buffer), "%.", val);

    // Precision without a format
    boost::decimal::snprintf(buffer, sizeof(buffer), "%.3", val);
}

int main()
{
    test_bootstrap<decimal32_t>();
    test_bootstrap<decimal64_t>();
    test_bootstrap<decimal128_t>();

    // Homebrew gcc on mac does not support locales
    #if !(defined(__APPLE__) && defined(__GNUC__) && !defined(__clang__)) && !defined(BOOST_DECIMAL_QEMU_TEST) && !defined(BOOST_DECIMAL_DISABLE_EXCEPTIONS)
    test_locales();
    #endif

    test_fuzzer_crash("");
    test_fuzzer_crash("Dd00000000001000000000000000000000000000000000001000000000ccccccccc�Cccc0ccccccccc8888000010000)001.2");

    test_bad_input<decimal32_t>();
    test_bad_input<decimal64_t>();
    test_bad_input<decimal128_t>();

    return boost::report_errors();
}
