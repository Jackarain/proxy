// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// See: https://github.com/boostorg/decimal/issues/1054

#if defined(__GNUC__) && (__GNUC__ >= 7)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

#ifndef BOOST_DECIMAL_USE_MODULE
#include <boost/decimal.hpp>
#endif

#include <boost/core/lightweight_test.hpp>
#include <iostream>

#ifdef BOOST_DECIMAL_USE_MODULE
import boost.decimal;
#endif

using namespace boost::decimal;

// https://en.cppreference.com/w/cpp/compiler_support/17
#if (defined(__GNUC__) && __GNUC__ >= 11) || \
((defined(__clang__) && __clang_major__ >= 14 && !defined(__APPLE__)) || (defined(__clang__) && defined(__APPLE__) && __clang_major__ >= 16)) || \
(defined(_MSC_VER) && _MSC_VER >= 1924)

#   if __cplusplus >= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
#       include <charconv>
#       define BOOST_DECIMAL_TEST_CHARCONV
#   endif

#endif

#ifdef BOOST_DECIMAL_TEST_CHARCONV

template <typename DecimalType>
void endptr_using_from_chars(const std::string& str)
{
    auto get_endptr_1 = [](const auto& s) {
        DecimalType x;
        return boost::decimal::from_chars(s.data(), s.data() + s.size(), x).ptr;
    };
    auto get_endptr_2 = [](const auto& s) {
        double x;
        return std::from_chars(s.data(), s.data() + s.size(), x).ptr;
    };
    BOOST_TEST_CSTR_EQ(get_endptr_1(str), get_endptr_2(str));
}

#else

template <typename DecimalType>
void endptr_using_from_chars(const std::string&) {}

#endif

template <typename DecimalType>
void endptr_using_strtod(const std::string& str)
{
    auto get_endptr_1 = [](const auto& s) {
        char* ptr;
        boost::decimal::strtod<DecimalType>(s.data(), &ptr);
        return ptr;
    };
    auto get_endptr_2 = [](const auto& s) {
        char* ptr;
        std::strtod(s.data(), &ptr);
        return ptr;
    };

    BOOST_TEST_CSTR_EQ(get_endptr_1(str), get_endptr_2(str));
}

template <typename DecimalType>
void check_endptr()
{
    // endptr should point to the character after "inf"
    endptr_using_from_chars<DecimalType>("info");
    endptr_using_strtod<DecimalType>("info");

    // endptr should points to the beginning of the string
    endptr_using_from_chars<DecimalType>("inch");
    endptr_using_strtod<DecimalType>("inch");

    // endptr should point to the character after "nan"
    endptr_using_from_chars<DecimalType>("nano");
    endptr_using_strtod<DecimalType>("nano");

    // endptr should point to the beginning of the string
    endptr_using_from_chars<DecimalType>("name");
    endptr_using_strtod<DecimalType>("name");

    #ifdef __APPLE__
    // Darwin's strtod works incorrectly for nan with payload, so skip the test.
    #else
    // endptr should point to the character after "nan(PAYLOAD)"
    endptr_using_from_chars<DecimalType>("nan(PAYLOAD)");
    endptr_using_strtod<DecimalType>("nan(PAYLOAD)");

    // endptr should point to the character after "nan(123)"
    endptr_using_from_chars<DecimalType>("nan(123)");
    endptr_using_strtod<DecimalType>("nan(123)");

    // endptr should point to the character after "nan"
    endptr_using_from_chars<DecimalType>("nan(..BAD..)");
    endptr_using_strtod<DecimalType>("nan(..BAD..)");
    #endif
}

int main()
{
    // Restrict testing to a small cohort of compilers that we know generate correct results
    #if defined(__GNUC__) && __GNUC__ >= 10 && !defined(__MINGW32__)

    check_endptr<decimal32_t>();
    check_endptr<decimal64_t>();
    check_endptr<decimal128_t>();
    check_endptr<decimal_fast32_t>();
    check_endptr<decimal_fast64_t>();
    check_endptr<decimal_fast128_t>();

    #endif

    return boost::report_errors();
}
