// Copyright Antony Polukhin, 2025-2026.
// Copyright Fedor Osetrov, 2025-2026.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

module;

#include <iostream>
#include <string>
#include <memory>

#include <my_plugin_api.hpp>

module sample_plugin;

namespace my_namespace {

class my_module_plugin final : public my_plugin_api {
public:
    my_module_plugin() {
        std::cout << "Constructing my_module_plugin" << std::endl;
    }

    std::string name() const override {
        return "module_plugin";
    }

    float calculate(float x, float y) override {
        return x - y;
    }

    ~my_module_plugin() {
        std::cout << "Destructing my_module_plugin" << std::endl;
    }
};

std::shared_ptr<my_plugin_api> create_plugin() {
    return std::make_shared<my_module_plugin>();
}

} // namespace my_namespace
