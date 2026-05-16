// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/decimal.hpp>
#include <iostream>

#if __cplusplus > 202002L || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)
#  if __has_include(<print>)
#    include <print>
#    if defined(__cpp_lib_print) && __cpp_lib_print >= 202207L
#      define BOOST_DECIMAL_HAS_PRINT_SUPPORT
#    endif
#  endif
#endif

#if defined(BOOST_DECIMAL_HAS_FORMAT_SUPPORT) && defined(BOOST_DECIMAL_HAS_PRINT_SUPPORT)

int main()
{
    constexpr boost::decimal::decimal64_t val1 {314, -2};
    constexpr boost::decimal::decimal32_t val2 {3141, -3};

    std::print("[{:>10.3e}]\n", val1);
    std::print("[{:*<10.3e}]\n", val2);

    return 0;
}

#else

int main()
{
    std::cout << "<print> is unsupported" << std::endl;
    return 0;
}

#endif
