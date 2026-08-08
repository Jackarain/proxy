// Copyright (c) 2025-2026 Antony Polukhin
// Copyright (c) 2025-2026 Fedor Osetrov
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <memory>
#include <string>

import boost.conversion;

namespace {

struct Base {
    virtual ~Base() = default;
    virtual std::string name() const {
        return "base";
    }
};

struct Derived : Base {
    std::string name() const override {
        return "derived";
    }
};

}

int main() {
    std::cerr << boost::implicit_cast<long>(42) << '\n';

    std::unique_ptr<Base> base = std::make_unique<Derived>();
    std::cerr << boost::polymorphic_downcast<Derived&>(*base).name() << '\n';

    return 0;
}
