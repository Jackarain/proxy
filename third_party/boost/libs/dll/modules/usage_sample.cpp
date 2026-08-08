// Copyright Antony Polukhin, 2025-2026.
// Copyright Fedor Osetrov, 2025-2026.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>

#include "../example/tutorial_common/my_plugin_api.hpp"

import boost.dll;

int main(int argc, const char* const argv[]) {
    namespace dll = boost::dll::experimental;

    dll::smart_library module_library(argv[1]);

    auto creator = module_library.get_function<std::shared_ptr<my_plugin_api>()>("my_namespace::create_plugin@sample_plugin");

    const float res = creator()->calculate(10, 8.5);
    std::cout << "Result: " << res << '\n';

    return 0;
}
