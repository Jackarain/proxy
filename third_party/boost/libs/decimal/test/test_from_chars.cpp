// Copyright 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

// Needed for operations with boost math
#define BOOST_DECIMAL_ALLOW_IMPLICIT_INTEGER_CONVERSIONS

#include "mini_to_chars.hpp"
#include <boost/decimal.hpp>

#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wold-style-cast"
#  pragma clang diagnostic ignored "-Wundef"
#  pragma clang diagnostic ignored "-Wconversion"
#  pragma clang diagnostic ignored "-Wsign-conversion"
#  pragma clang diagnostic ignored "-Wfloat-equal"
#  if __clang_major__ >= 20
#    pragma clang diagnostic ignored "-Wfortify-source"
#  endif
#elif defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wold-style-cast"
#  pragma GCC diagnostic ignored "-Wundef"
#  pragma GCC diagnostic ignored "-Wconversion"
#  pragma GCC diagnostic ignored "-Wsign-conversion"
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#  pragma GCC diagnostic ignored "-Wuseless-cast"
#endif

#include <boost/charconv.hpp>
#include <boost/math/special_functions/next.hpp>
#include <boost/core/lightweight_test.hpp>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <random>
#include <cwchar>

using namespace boost::decimal;

static std::mt19937_64 rng(42);

#if !defined(BOOST_DECIMAL_REDUCE_TEST_DEPTH)
static constexpr auto N = static_cast<std::size_t>(1024); // Number of trials
#else
static constexpr auto N = static_cast<std::size_t>(1024 >> 4U); // Number of trials
#endif

#if !defined(BOOST_DECIMAL_DISABLE_CLIB)

template <typename T>
void test_from_chars_scientific()
{
    std::uniform_real_distribution<float> dist(1e-10F, 1e10F);

    constexpr auto max_iter {std::is_same<T, decimal128_t>::value ? N / 4 : N};

    for (std::size_t i {}; i < max_iter; ++i)
    {
        char buffer[256] {};
        const auto val {dist(rng)};
        auto r = boost::charconv::to_chars(buffer, buffer + sizeof(buffer), val, boost::charconv::chars_format::scientific);

        if (!r)
        {
            continue; // LCOV_EXCL_LINE
        }

        *r.ptr = '\0';

        T return_value;
        const auto r_dec = from_chars(buffer, buffer + std::strlen(buffer), return_value, chars_format::scientific);
        const auto ret_value_float = static_cast<float>(return_value);
        const auto float_distance = std::abs(boost::math::float_distance(ret_value_float, val));

        if (!(BOOST_TEST(float_distance <= 10) && BOOST_TEST(r_dec)))
        {
            // LCOV_EXCL_START
            std::cerr << "     Value: " << val
                      << "\n    Buffer: " << buffer
                      << "\n   Ret Val: " << return_value
                      << "\nFloat dist: " << float_distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }
}

template <typename T>
void test_from_chars_fixed()
{
    std::uniform_real_distribution<float> dist(1e-10F, 1e10F);

    constexpr auto max_iter {std::is_same<T, decimal128_t>::value ? N / 4 : N};

    for (std::size_t i {}; i < max_iter; ++i)
    {
        char buffer[256] {};
        const auto val {dist(rng)};
        auto r = boost::charconv::to_chars(buffer, buffer + sizeof(buffer), val, boost::charconv::chars_format::fixed);

        if (!r)
        {
            continue; // LCOV_EXCL_LINE
        }

        *r.ptr = '\0';

        T return_value;
        const auto r_dec = from_chars(buffer, buffer + std::strlen(buffer), return_value, chars_format::fixed);

        const auto ret_value_float = static_cast<float>(return_value);
        const auto float_distance = std::abs(boost::math::float_distance(ret_value_float, val));

        if (!(BOOST_TEST(float_distance <= 10) && BOOST_TEST(r_dec)))
        {
            // LCOV_EXCL_START
            std::cerr << "     Value: " << val
                      << "\n    Buffer: " << buffer
                      << "\n   Ret Val: " << return_value
                      << "\nFloat dist: " << float_distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }
}

template <typename T>
void test_from_chars_general()
{
    std::uniform_real_distribution<float> dist(1e-10F, 1e10F);

    constexpr auto max_iter {std::is_same<T, decimal128_t>::value ? N / 4 : N};

    for (std::size_t i {}; i < max_iter; ++i)
    {
        char buffer[256] {};
        const auto val {dist(rng)};
        auto r = boost::charconv::to_chars(buffer, buffer + sizeof(buffer), val, boost::charconv::chars_format::general);

        if (!r)
        {
            continue; // LCOV_EXCL_LINE
        }

        *r.ptr = '\0';

        T return_value;
        const auto r_dec = from_chars(buffer, buffer + std::strlen(buffer), return_value, chars_format::general);
        const auto ret_value_float = static_cast<float>(return_value);
        const auto float_distance = std::abs(boost::math::float_distance(ret_value_float, val));

        if (!(BOOST_TEST(float_distance <= 10) && BOOST_TEST(r_dec)))
        {
            // LCOV_EXCL_START
            std::cerr << "     Value: " << val
                      << "\n    Buffer: " << buffer
                      << "\n   Ret Val: " << return_value
                      << "\nFloat dist: " << float_distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }
}

template <typename T>
void test_non_finite_values()
{
    T val;
    BOOST_TEST(from_chars(nullptr, nullptr, val).ec == std::errc::invalid_argument);

    const char* snan_str = "nan(snan)";
    auto r = from_chars(snan_str, snan_str + std::strlen(snan_str), val);
    BOOST_TEST(r);
    BOOST_TEST(isnan(val));

    const char* qnan_str = "nan";
    val = 0;
    r = from_chars(qnan_str, qnan_str + std::strlen(qnan_str), val);
    BOOST_TEST(r);
    BOOST_TEST(isnan(val));

    const char* inf_str = "inf";
    val = 0;
    r = from_chars(inf_str, inf_str + std::strlen(inf_str), val);
    BOOST_TEST(r);
    BOOST_TEST(isinf(val));
}

template <typename T>
void test_hex_values()
{
    const char* buffer1 = "1.3a2bp-10";
    T v1;
    auto r1 = from_chars(buffer1, buffer1 + std::strlen(buffer1), v1, chars_format::hex);
    BOOST_TEST(r1.ec == std::errc());
    constexpr auto res_1 = T{UINT64_C(80427), -14, false};
    BOOST_TEST_EQ(v1, res_1);

    const char* buffer2 = "1.234p-10";
    T v2;
    auto r2 = from_chars(buffer2, buffer2 + std::strlen(buffer2), v2, chars_format::hex);
    BOOST_TEST(r2.ec == std::errc());
    constexpr auto res_2 = T{UINT64_C(4660), -13, false};
    BOOST_TEST_EQ(v2, res_2);

    const char* buffer3 = "-2a";
    T v3;
    auto r3 = from_chars(buffer3, buffer3 + std::strlen(buffer3), v3, chars_format::hex);
    BOOST_TEST(r3.ec == std::errc());
    constexpr auto res_3 = T{UINT64_C(42), 0, true};
    BOOST_TEST_EQ(v3, res_3);
}

#ifdef BOOST_DECIMAL_HAS_STD_CHARCONV

template <typename T>
void test_from_chars_scientific_std()
{
    std::uniform_real_distribution<float> dist(1e-10F, 1e10F);

    constexpr auto max_iter {std::is_same<T, decimal128_t>::value ? N / 4 : N};

    for (std::size_t i {}; i < max_iter; ++i)
    {
        char buffer[256] {};
        const auto val {dist(rng)};
        auto r = boost::charconv::to_chars(buffer, buffer + sizeof(buffer), val, boost::charconv::chars_format::scientific);

        if (!r)
        {
            continue; // LCOV_EXCL_LINE
        }

        *r.ptr = '\0';

        T return_value;
        const std::from_chars_result r_dec = from_chars(buffer, buffer + std::strlen(buffer), return_value, std::chars_format::scientific);
        const auto ret_value_float = static_cast<float>(return_value);
        const auto float_distance = std::abs(boost::math::float_distance(ret_value_float, val));

        if (!(BOOST_TEST(float_distance <= 10) && BOOST_TEST(r_dec.ec == std::errc())))
        {
            // LCOV_EXCL_START
            std::cerr << "     Value: " << val
                      << "\n    Buffer: " << buffer
                      << "\n   Ret Val: " << return_value
                      << "\nFloat dist: " << float_distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }
}

template <typename T>
void test_from_chars_fixed_std()
{
    std::uniform_real_distribution<float> dist(1e-10F, 1e10F);

    constexpr auto max_iter {std::is_same<T, decimal128_t>::value ? N / 4 : N};

    for (std::size_t i {}; i < max_iter; ++i)
    {
        char buffer[256] {};
        const auto val {dist(rng)};
        auto r = boost::charconv::to_chars(buffer, buffer + sizeof(buffer), val, boost::charconv::chars_format::fixed);

        if (!r)
        {
            continue; // LCOV_EXCL_LINE
        }

        *r.ptr = '\0';

        T return_value;
        const std::from_chars_result r_dec = from_chars(buffer, buffer + std::strlen(buffer), return_value, std::chars_format::fixed);

        const auto ret_value_float = static_cast<float>(return_value);
        const auto float_distance = std::abs(boost::math::float_distance(ret_value_float, val));

        if (!(BOOST_TEST(float_distance <= 10) && BOOST_TEST(r_dec.ec == std::errc())))
        {
            // LCOV_EXCL_START
            std::cerr << "     Value: " << val
                      << "\n    Buffer: " << buffer
                      << "\n   Ret Val: " << return_value
                      << "\nFloat dist: " << float_distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }
}

template <typename T>
void test_from_chars_general_std()
{
    std::uniform_real_distribution<float> dist(1e-10F, 1e10F);

    constexpr auto max_iter {std::is_same<T, decimal128_t>::value ? N / 4 : N};

    for (std::size_t i {}; i < max_iter; ++i)
    {
        char buffer[256] {};
        const auto val {dist(rng)};
        auto r = boost::charconv::to_chars(buffer, buffer + sizeof(buffer), val, boost::charconv::chars_format::general);

        if (!r)
        {
            continue; // LCOV_EXCL_LINE
        }

        *r.ptr = '\0';

        T return_value;
        const std::from_chars_result r_dec = from_chars(buffer, buffer + std::strlen(buffer), return_value, std::chars_format::general);
        const auto ret_value_float = static_cast<float>(return_value);
        const auto float_distance = std::abs(boost::math::float_distance(ret_value_float, val));

        if (!(BOOST_TEST(float_distance <= 10) && BOOST_TEST(r_dec.ec == std::errc())))
        {
            // LCOV_EXCL_START
            std::cerr << "     Value: " << val
                      << "\n    Buffer: " << buffer
                      << "\n   Ret Val: " << return_value
                      << "\nFloat dist: " << float_distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }
}

#endif

template <typename T>
void test_string_interface()
{
    constexpr T correct_val {42};
    std::string str {"42"};
    T val;
    auto r = from_chars(str, val);
    BOOST_TEST(r);
    BOOST_TEST_EQ(val, correct_val);

    // Empty string
    std::string empty;
    r = from_chars(empty, val);
    BOOST_TEST(r.ec == std::errc::invalid_argument);

    #ifdef BOOST_DECIMAL_HAS_STD_STRING_VIEW
    std::string_view empty_view = std::string_view(empty);
    r = from_chars(empty_view, val);
    BOOST_TEST(r.ec == std::errc::invalid_argument);
    #endif
}

int main()
{
    test_from_chars_scientific<decimal32_t>();
    test_from_chars_scientific<decimal64_t>();
    test_from_chars_scientific<decimal_fast32_t>();
    test_from_chars_scientific<decimal_fast64_t>();

    test_from_chars_fixed<decimal32_t>();
    test_from_chars_fixed<decimal64_t>();
    test_from_chars_fixed<decimal_fast32_t>();
    test_from_chars_fixed<decimal_fast64_t>();

    test_from_chars_general<decimal32_t>();
    test_from_chars_general<decimal64_t>();
    test_from_chars_general<decimal_fast32_t>();
    test_from_chars_general<decimal_fast64_t>();

    #ifdef BOOST_DECIMAL_HAS_STD_CHARCONV
    test_from_chars_scientific_std<decimal32_t>();
    test_from_chars_scientific_std<decimal64_t>();
    test_from_chars_scientific_std<decimal_fast32_t>();
    test_from_chars_scientific_std<decimal_fast64_t>();

    test_from_chars_fixed_std<decimal32_t>();
    test_from_chars_fixed_std<decimal64_t>();
    test_from_chars_fixed_std<decimal_fast32_t>();
    test_from_chars_fixed_std<decimal_fast64_t>();

    test_from_chars_general_std<decimal32_t>();
    test_from_chars_general_std<decimal64_t>();
    test_from_chars_general_std<decimal_fast32_t>();
    test_from_chars_general_std<decimal_fast64_t>();
    #endif

    test_non_finite_values<decimal32_t>();
    test_non_finite_values<decimal64_t>();
    test_non_finite_values<decimal_fast32_t>();
    test_non_finite_values<decimal_fast64_t>();

    test_hex_values<decimal32_t>();
    test_hex_values<decimal64_t>();
    test_hex_values<decimal_fast32_t>();
    test_hex_values<decimal_fast64_t>();

    test_string_interface<decimal32_t>();
    test_string_interface<decimal64_t>();
    test_string_interface<decimal_fast32_t>();
    test_string_interface<decimal_fast64_t>();

    #if !defined(BOOST_DECIMAL_REDUCE_TEST_DEPTH)
    test_from_chars_scientific<decimal128_t>();
    test_from_chars_fixed<decimal128_t>();
    test_from_chars_general<decimal128_t>();
    test_non_finite_values<decimal128_t>();
    test_hex_values<decimal128_t>();
    test_string_interface<decimal128_t>();

    test_from_chars_scientific<decimal_fast128_t>();
    test_from_chars_fixed<decimal_fast128_t>();
    test_from_chars_general<decimal_fast128_t>();
    test_non_finite_values<decimal_fast128_t>();
    test_hex_values<decimal_fast128_t>();
    test_string_interface<decimal_fast128_t>();
    #endif

    return boost::report_errors();
}

#else

int main()
{
    return 0;
}

#endif
