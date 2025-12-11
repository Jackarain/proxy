// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <memory>

#include <boost/openmethod.hpp>
#include <boost/openmethod/inplace_vptr.hpp>
#include <boost/openmethod/initialize.hpp>

using namespace boost::openmethod;

struct Animal : inplace_vptr_base<Animal> {};

struct Cat : Animal, inplace_vptr_derived<Cat, Animal> {};

struct Dog : Animal, inplace_vptr_derived<Dog, Animal> {};

BOOST_OPENMETHOD(
    poke, (virtual_<Animal&> animal, std::ostream& os), void);

BOOST_OPENMETHOD_OVERRIDE(poke, (Cat&, std::ostream& os), void) {
    os << "hiss\n";
}

BOOST_OPENMETHOD_OVERRIDE(poke, (Dog&, std::ostream& os), void) {
    os << "bark\n";
}

int main() {
    initialize();

    std::unique_ptr<Animal> a = std::make_unique<Cat>();
    std::unique_ptr<Animal> b = std::make_unique<Dog>();

    poke(*a, std::cout); // hiss
    poke(*b, std::cout); // bark

    return 0;
}
