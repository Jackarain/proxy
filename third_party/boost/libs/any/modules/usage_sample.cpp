// Copyright (c) 2016-2025 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// To compile manually use a command like the following:
// clang++ -std=c++20 -fmodule-file=type_index.pcm type_index.pcm usage_sample.cpp

//[any_module_example
import boost.any;

int main() {
    boost::any a = 42;
}
//]

