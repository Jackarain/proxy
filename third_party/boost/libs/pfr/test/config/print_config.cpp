// Copyright (c) 2016-2025 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/pfr/config.hpp>
#if BOOST_PFR_ENABLED
#include <boost/pfr.hpp>
#endif
#include <boost/preprocessor/stringize.hpp>

#include <iostream>

int main() {
    std::cout << "Platform info:" << '\n'
        << "BOOST_PFR_USE_CPP17 == " << BOOST_PFR_USE_CPP17 << '\n'
        << "BOOST_PFR_USE_LOOPHOLE == " << BOOST_PFR_USE_LOOPHOLE << '\n'
        << "BOOST_PFR_USE_STD_MAKE_INTEGRAL_SEQUENCE == " << BOOST_PFR_USE_STD_MAKE_INTEGRAL_SEQUENCE << '\n'
        << "BOOST_PFR_HAS_GUARANTEED_COPY_ELISION == " << BOOST_PFR_HAS_GUARANTEED_COPY_ELISION << '\n'
        << "BOOST_PFR_ENABLE_IMPLICIT_REFLECTION == " << BOOST_PFR_ENABLE_IMPLICIT_REFLECTION << '\n'
        << "BOOST_PFR_CORE_NAME_ENABLED == " << BOOST_PFR_CORE_NAME_ENABLED << '\n'
        << "BOOST_PFR_FUNCTION_SIGNATURE == " << BOOST_PP_STRINGIZE(BOOST_PFR_FUNCTION_SIGNATURE) << '\n'
        << "BOOST_PFR_CORE_NAME_PARSING == " << BOOST_PP_STRINGIZE(BOOST_PFR_CORE_NAME_PARSING) << '\n'
        << "BOOST_PFR_ENABLED == " << BOOST_PFR_ENABLED << '\n'
        << "__cplusplus == " << __cplusplus << '\n'
#ifdef __cpp_structured_bindings
        << "__cpp_structured_bindings == " << __cpp_structured_bindings << '\n'
#endif
#ifdef _MSC_VER
        << "_MSC_VER == " << _MSC_VER << '\n'
#endif
#ifdef _MSVC_LANG
        << "_MSVC_LANG == " << _MSVC_LANG << '\n'
#endif
#ifdef __GLIBCXX__
        << "__GLIBCXX__ == " << __GLIBCXX__ << '\n'
#endif
#ifdef __GNUC__
        << "__GNUC__ == " << __GNUC__ << '\n'
#endif
#ifdef __clang_major__
        << "__clang_major__ == " << __clang_major__ << '\n'
#endif
    ;
}
