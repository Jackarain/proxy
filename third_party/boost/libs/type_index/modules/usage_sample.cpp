// Copyright (c) 2016-2025 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// To compile manually use a command like the folowing:
// clang++ -std=c++20 -fmodule-file=type_index.pcm type_index.pcm usage_sample.cpp

//[type_index_module_example
#include <iostream>

import boost.type_index;

int main() {
    std::cout << boost::typeindex::type_id_with_cvr<const int>();  // Outputs: const int
}
//]

