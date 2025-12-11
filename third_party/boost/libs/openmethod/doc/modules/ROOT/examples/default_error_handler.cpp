// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// tag::example[]
#include <iostream>
#include <variant>

#include <boost/openmethod.hpp>
#include <boost/openmethod/initialize.hpp>

using boost::openmethod::virtual_ptr;

struct Animal {
    virtual ~Animal() = default;
};

struct Cat : Animal {};
struct Dog : Animal {};

BOOST_OPENMETHOD_CLASSES(Animal, Cat, Dog);

BOOST_OPENMETHOD(trick, (std::ostream&, virtual_ptr<Animal>), void);

BOOST_OPENMETHOD_OVERRIDE(
    trick, (std::ostream & os, virtual_ptr<Dog> /*dog*/), void) {
    os << "spin\n";
}

auto main() -> int {
    namespace bom = boost::openmethod;
    bom::initialize();

    bom::default_registry::error_handler::set([](const auto& error) {
        if (std::holds_alternative<bom::no_overrider>(error)) {
            throw std::runtime_error("not implemented");
        }
    });

    Cat felix;
    Dog hector, snoopy;
    std::vector<Animal*> animals = {&hector, &felix, &snoopy};

    for (auto animal : animals) {
        try {
            trick(std::cout, *animal);
        } catch (std::runtime_error& error) {
            std::cerr << error.what() << "\n";
        }
    }

    return 0;
}
// end::example[]
