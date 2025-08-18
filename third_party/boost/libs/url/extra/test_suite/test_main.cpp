//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#include "test_suite.hpp"

int
main(int argc, char const* const* argv)
{
    // Custom
    // Enable memory leak checking in debug mode on MSVC (not Clang)
#if defined(_MSC_VER) && !defined(__clang__)
    // Get the current debug flag settings
    int flags = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
    // Enable automatic memory leak check at program exit
    flags |= _CRTDBG_LEAK_CHECK_DF;
    // Set the updated debug flag settings
    _CrtSetDbgFlag(flags);
#endif
    return ::test_suite::run(argc, argv);
}
