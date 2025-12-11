// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// tag::example[]
#include <iostream>

#include <boost/openmethod/default_registry.hpp>

struct Animal {
    virtual ~Animal() = default;
};

struct Cat : Animal {};
struct Dog : Animal {};

namespace bom = boost::openmethod;

struct throw_if_not_implemented : bom::policies::error_handler {
    template<class Registry>
    struct fn {
        static auto error(const bom::openmethod_error&) -> void {
        }

        static auto error(const bom::no_overrider& err) -> void {
            throw err;
        }
    };
};

struct custom_registry : bom::default_registry::with<throw_if_not_implemented> {
};

#define BOOST_OPENMETHOD_DEFAULT_REGISTRY custom_registry

#include <boost/openmethod.hpp>
#include <boost/openmethod/initialize.hpp>

using boost::openmethod::virtual_ptr;

BOOST_OPENMETHOD_CLASSES(Animal, Cat, Dog);

BOOST_OPENMETHOD(trick, (std::ostream&, virtual_ptr<Animal>), void);

BOOST_OPENMETHOD_OVERRIDE(
    trick, (std::ostream & os, virtual_ptr<Dog> /*dog*/), void) {
    os << "spin\n";
}

auto main() -> int {
    bom::initialize();

    Cat felix;
    Dog hector, snoopy;
    std::vector<Animal*> animals = {&hector, &felix, &snoopy};

    for (auto animal : animals) {
        try {
            trick(std::cout, *animal);
        } catch (bom::no_overrider&) {
            std::cout << "not implemented\n";
        }
    }

    return 0;
}
// end::example[]
