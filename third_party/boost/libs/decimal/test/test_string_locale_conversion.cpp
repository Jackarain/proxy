// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/decimal/detail/config.hpp>

#if !(defined(__GNUC__) && __GNUC__ >= 5 && defined(__APPLE__)) && !defined(BOOST_DECIMAL_QEMU_TEST) && !defined(BOOST_DECIMAL_DISABLE_EXCEPTIONS) && !defined(_MSC_VER)

#include <boost/decimal/detail/locale_conversion.hpp>
#include <boost/core/lightweight_test.hpp>
#include <iostream>
#include <iomanip>
#include <locale>
#include <sstream>

using namespace boost::decimal::detail;

void test_conversion_to_c_locale(const char* locale)
{
    try
    {
        const std::locale a(locale);
        std::locale::global(a);

        std::stringstream out_double;
        out_double.imbue(a);
        out_double << 1122.89;

        char buffer[64] {};
        std::memcpy(buffer, out_double.str().c_str(), out_double.str().size());
        convert_string_to_c_locale(buffer);

        const auto res = "1122.89";
        BOOST_TEST_CSTR_EQ(buffer, res);
    }
    // LCOV_EXCL_START
    catch (...)
    {
        std::cerr << "Test not run" << std::endl;
    }
    // LCOV_EXCL_STOP
}

void test_conversion_from_c_locale(const char* locale, const char* res)
{
    try
    {
        const std::locale a(locale);
        std::locale::global(a);

        const auto str = "1122.89";
        char buffer[64] {};
        std::memcpy(buffer, str, strlen(str));
        char buffer2[64] {};
        std::memcpy(buffer2, str, strlen(str));

        convert_pointer_pair_to_local_locale(buffer2, buffer2 + sizeof(buffer2));
        BOOST_TEST_CSTR_EQ(buffer2, res);
    }
    // LCOV_EXCL_START
    catch (...)
    {
        std::cerr << "Test not run" << std::endl;
    }
    // LCOV_EXCL_STOP
}

int main()
{
    test_conversion_to_c_locale("en_US.UTF-8"); // . decimal, , thousands
    test_conversion_to_c_locale("de_DE.UTF-8"); // , decimal, . thousands
    #if (defined(__clang__) && __clang_major__ > 9) || (defined(__GNUC__) && __GNUC__ > 9)
    test_conversion_to_c_locale("fr_FR.UTF-8"); // , decimal,   thousands
    #endif

    test_conversion_from_c_locale("en_US.UTF-8", "1,122.89");

    #if !defined(__APPLE__) || (defined(__APPLE__) && defined(__clang__) && __clang_major__ > 15)
    test_conversion_from_c_locale("de_DE.UTF-8", "1.122,89");
    test_conversion_from_c_locale("fr_FR.UTF-8", "1 122,89");
    #endif

    return boost::report_errors();
}

#else

int main()
{
    return 0;
}

#endif
