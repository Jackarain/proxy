//
// Copyright (c) 2009-2011 Artyom Beilis (Tonkikh)
//
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_LOCALE_UNIT_TEST_HPP
#define BOOST_LOCALE_UNIT_TEST_HPP

#include <boost/locale/config.hpp>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

int error_counter = 0;
int test_counter = 0;

#ifndef BOOST_LOCALE_ERROR_LIMIT
#    define BOOST_LOCALE_ERROR_LIMIT 20
#endif

#define BOOST_LOCALE_STRINGIZE(x) #x

#define THROW_IF_TOO_BIG(X)            \
    if((X) > BOOST_LOCALE_ERROR_LIMIT) \
    throw std::runtime_error("Error limits reached, stopping unit test")

#define TEST(X)                                                           \
    do {                                                                  \
        test_counter++;                                                   \
        if(X)                                                             \
            break;                                                        \
        std::cerr << "Error in line:" << __LINE__ << " " #X << std::endl; \
        THROW_IF_TOO_BIG(error_counter++);                                \
        BOOST_LOCALE_START_CONST_CONDITION                                \
    } while(0) BOOST_LOCALE_END_CONST_CONDITION

#define TEST_REQUIRE(X)                                                    \
    do {                                                                   \
        test_counter++;                                                    \
        if(X)                                                              \
            break;                                                         \
        std::cerr << "Error in line " << __LINE__ << ": " #X << std::endl; \
        throw std::runtime_error("Critical test " #X " failed");           \
        BOOST_LOCALE_START_CONST_CONDITION                                 \
    } while(0) BOOST_LOCALE_END_CONST_CONDITION

#define TEST_THROWS(X, E)                                                  \
    do {                                                                   \
        test_counter++;                                                    \
        try {                                                              \
            X;                                                             \
        } catch(E const& /*e*/) {                                          \
            break;                                                         \
        } catch(...) {                                                     \
        }                                                                  \
        std::cerr << "Error in line " << __LINE__ << ": " #X << std::endl; \
        THROW_IF_TOO_BIG(error_counter++);                                 \
        BOOST_LOCALE_START_CONST_CONDITION                                 \
    } while(0) BOOST_LOCALE_END_CONST_CONDITION

void test_main(int argc, char** argv);

int main(int argc, char** argv)
{
    try {
        test_main(argc, argv);
    } catch(const std::exception& e) {
        std::cerr << "Failed " << e.what() << std::endl; // LCOV_EXCL_LINE
        return EXIT_FAILURE;                             // LCOV_EXCL_LINE
    }
    int passed = test_counter - error_counter;
    std::cout << std::endl;
    std::cout << "Passed " << passed << " tests\n";
    if(error_counter > 0) {
        std::cout << "Failed " << error_counter << " tests\n"; // LCOV_EXCL_LINE
    }
    std::cout << " " << std::fixed << std::setprecision(1) << std::setw(5) << 100.0 * passed / test_counter
              << "% of tests completed successfully\n";
    return error_counter == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

template<typename T>
std::string to_string(T const& s)
{
    std::stringstream ss;
    ss << s;
    return ss.str();
}

const std::string& to_string(const std::string& s)
{
    return s;
}

/// Put the char into the stream making sure it is readable
/// Fallback to the unicode representation of it (e.g. U+00A0)
template<typename Char>
void stream_char(std::ostream& s, const Char c)
{
    if((c >= '!' && c <= '~') || c == ' ')
        s << static_cast<char>(c);
    else
        s << "U+" << std::hex << std::uppercase << std::setw(sizeof(Char)) << static_cast<unsigned>(c);
}

template<typename Char>
std::string to_string(const std::basic_string<Char>& s)
{
    std::stringstream ss;
    for(size_t i = 0; i < s.size(); ++i)
        stream_char(ss, s[i]);
    return ss.str();
}

// Unicode chars cannot be streamed directly (deleted overloads in C++20)
template<typename Char>
std::string to_string_char_impl(const Char c)
{
    std::stringstream ss;
    stream_char(ss, c);
    return ss.str();
}

std::string to_string(const wchar_t c)
{
    return to_string_char_impl(c);
}
std::string to_string(const char16_t c)
{
    return to_string_char_impl(c);
}
std::string to_string(const char32_t c)
{
    return to_string_char_impl(c);
}

template<typename T, typename U>
void test_impl(bool success, T const& l, U const& r, const char* expr, const char* fail_expr, int line)
{
    test_counter++;
    if(!success) {
        std::cerr << "Error in line " << line << ": " << expr << std::endl;
        std::cerr << "---- [" << to_string(l) << "] " << fail_expr << " [" << to_string(r) << "]" << std::endl;
        THROW_IF_TOO_BIG(error_counter++);
    }
}

template<typename T, typename U>
void test_eq_impl(T const& l, U const& r, const char* expr, int line)
{
    test_impl(l == r, l, r, expr, "!=", line);
}

template<typename T, typename U>
void test_le_impl(T const& l, U const& r, const char* expr, int line)
{
    test_impl(l <= r, l, r, expr, ">", line);
}

template<typename T, typename U>
void test_ge_impl(T const& l, U const& r, const char* expr, int line)
{
    test_impl(l >= r, l, r, expr, "<", line);
}

#define TEST_EQ(x, y) test_eq_impl(x, y, BOOST_LOCALE_STRINGIZE(x == y), __LINE__)
#define TEST_LE(x, y) test_le_impl(x, y, BOOST_LOCALE_STRINGIZE(x <= y), __LINE__)
#define TEST_GE(x, y) test_ge_impl(x, y, BOOST_LOCALE_STRINGIZE(x >= y), __LINE__)

#ifdef BOOST_MSVC
#    define BOOST_LOCALE_DISABLE_UNREACHABLE_CODE_WARNING __pragma(warning(disable : 4702))
#else
#    define BOOST_LOCALE_DISABLE_UNREACHABLE_CODE_WARNING
#endif

#endif
