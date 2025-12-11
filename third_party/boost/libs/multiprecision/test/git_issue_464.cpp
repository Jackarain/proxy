///////////////////////////////////////////////////////////////////////////////
//  Copyright 2022 - 2025 Matt Borland.
//  Copyright 2025 Christopher Kormanyos.
//  Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
//  See: https://github.com/boostorg/multiprecision/issues/464

#include <test.hpp>

#include <boost/multiprecision/cpp_dec_float.hpp>

#include <sstream>
#include <string>

template <typename T>
void test()
{
    auto a = boost::multiprecision::cpp_dec_float_50 { 12345 };

    auto d1 = a.convert_to<T>();

    {
        std::stringstream strm { };
        strm.imbue(std::locale::classic());
        strm << std::scientific << std::setprecision(8) << d1;

        BOOST_CHECK(strm.str().find("1.2345") != std::string::npos);
    }

    #if defined(BOOST_MSVC)
    // Ubuntu runners did not have this locale in GCC.
    // Windows runners seem like they do have this locale in MSVC.
    // I did not check any others

    // Set a different locale.
    std::locale::global(std::locale("de_DE"));
    #endif

    auto d2 = a.convert_to<T>();

    {
        std::stringstream strm { };
        strm.imbue(std::locale::classic());
        strm << std::scientific << std::setprecision(8) << d2;

        BOOST_CHECK(strm.str().find("1.2345") != std::string::npos);
    }

    BOOST_CHECK_EQUAL(d1, d2);
}

int main(void)
{
    test<double>();
    test<long double>();

    return boost::report_errors();
}
