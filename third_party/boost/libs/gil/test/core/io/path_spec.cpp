//
// Copyright 2024 Dirk Stolle
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/core/lightweight_test.hpp>
#include <boost/gil/io/path_spec.hpp>
#include <cstring>
#include <locale>
#include <string>

namespace gil = boost::gil;

void test_convert_to_string_from_wstring()
{
    std::wstring const path = L"/some_path/傳/привет/qwerty";
    std::string const expected = "/some_path/\xE5\x82\xB3/\xD0\xBF\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82/qwerty";

    std::string string = gil::detail::convert_to_string(path);
    BOOST_TEST_EQ( 34, string.size() );
    BOOST_TEST_EQ( expected, string );
}

void test_convert_to_native_string_from_wchar_t_ptr()
{
    wchar_t const* path = L"/some_path/傳/привет/qwerty";
    char const* expected = "/some_path/\xE5\x82\xB3/\xD0\xBF\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82/qwerty";

    char const* string = gil::detail::convert_to_native_string(path);
    BOOST_TEST_EQ( 34, strlen(string) );
    BOOST_TEST_EQ( 0, std::strcmp(expected, string) );
    delete[] string;
}

void test_convert_to_native_string_from_wstring()
{
    std::wstring const path = L"/some_path/傳/привет/qwerty";
    char const* expected = "/some_path/\xE5\x82\xB3/\xD0\xBF\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82/qwerty";

    char const* string = gil::detail::convert_to_native_string(path);
    BOOST_TEST_EQ( 34, strlen(string) );
    BOOST_TEST_EQ( 0, std::strcmp(expected, string) );
    delete[] string;
}

int main()
{
    // Set global locale to one that uses UTF-8. Could be "en_US.UTF-8" or
    // "C.UTF-8" or something similar, as long as it exists on the system.
    std::locale::global(std::locale("C.UTF-8"));

    test_convert_to_string_from_wstring();
    test_convert_to_native_string_from_wchar_t_ptr();
    test_convert_to_native_string_from_wstring();

    return boost::report_errors();
}
