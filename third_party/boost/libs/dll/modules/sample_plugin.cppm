// Copyright Antony Polukhin, 2025-2026.
// Copyright Fedor Osetrov, 2025-2026.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

module;

#include <memory>

#include <my_plugin_api.hpp>

export module sample_plugin;

namespace my_namespace {
    export std::shared_ptr<my_plugin_api> create_plugin();
} // namespace my_namespace
