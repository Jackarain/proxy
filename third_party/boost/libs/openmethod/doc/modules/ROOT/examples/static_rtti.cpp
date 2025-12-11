// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

//[ all

#include <boost/openmethod/default_registry.hpp>
#include <boost/openmethod/policies/static_rtti.hpp>

struct static_registry
    : boost::openmethod::registry<boost::openmethod::policies::static_rtti> {};

#define BOOST_OPENMETHOD_DEFAULT_REGISTRY static_registry

#include <boost/openmethod.hpp>
#include <boost/openmethod/interop/std_unique_ptr.hpp>
#include <boost/openmethod/initialize.hpp>

#include <iostream>

struct Animal {};

struct Dog : Animal {};

struct Cat : Animal {};

using namespace boost::openmethod::aliases;

BOOST_OPENMETHOD_CLASSES(Animal, Dog, Cat);

BOOST_OPENMETHOD(poke, (virtual_ptr<Animal>, std::ostream&), void);

BOOST_OPENMETHOD_OVERRIDE(
    poke, (virtual_ptr<Dog> dog, std::ostream& os), void) {
    os << "bark\n";
}

BOOST_OPENMETHOD_OVERRIDE(
    poke, (virtual_ptr<Cat> cat, std::ostream& os), void) {
    os << "hiss\n";
}

int main() {
    boost::openmethod::initialize();

    unique_virtual_ptr<Animal> a = make_unique_virtual<Cat>(),
                               b = make_unique_virtual<Dog>();

    poke(a, std::cout); // hiss
    poke(b, std::cout); // bark

    return 0;
}

//]
