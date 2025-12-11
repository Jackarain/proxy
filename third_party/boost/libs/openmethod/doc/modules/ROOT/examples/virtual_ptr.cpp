// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <boost/openmethod.hpp>
#include <boost/openmethod/initialize.hpp>

using boost::openmethod::virtual_ptr;

struct Animal {
    virtual ~Animal() = default;
};

struct Cat : Animal {};

struct Dog : Animal {};

BOOST_OPENMETHOD_CLASSES(Animal, Cat, Dog);

BOOST_OPENMETHOD(yell, (std::ostream&, virtual_ptr<Animal>), void);

BOOST_OPENMETHOD_OVERRIDE(
    yell, (std::ostream & os, virtual_ptr<Cat> /*cat*/), void) {
    os << "hiss";
}

BOOST_OPENMETHOD_OVERRIDE(
    yell, (std::ostream & os, virtual_ptr<Dog> /*dog*/), void) {
    os << "bark";
}

BOOST_OPENMETHOD(
    encounter, (std::ostream&, virtual_ptr<Animal>, virtual_ptr<Animal>), void);

BOOST_OPENMETHOD_OVERRIDE(
    encounter,
    (std::ostream & os, virtual_ptr<Dog> dog, virtual_ptr<Cat> /*cat*/), void) {
    yell(os, dog);
    os << " and chase";
}

BOOST_OPENMETHOD_OVERRIDE(
    encounter,
    (std::ostream & os, virtual_ptr<Cat> cat, virtual_ptr<Dog> /*dog*/), void) {
    yell(os, cat);
    os << " and run";
}

auto main() -> int {
    boost::openmethod::initialize();

    std::unique_ptr<Animal> a(new Cat);
    std::unique_ptr<Animal> b(new Dog);

    encounter(std::cout, *a, *b); // prints "hiss and run"
    std::cout << "\n";

    encounter(std::cout, *b, *a); // prints "bark and chase"
    std::cout << "\n";

    return 0;
}

void call_yell(std::ostream& os, virtual_ptr<Animal> a) {
    yell(os, a);
}

void call_encounter(
    std::ostream& os, virtual_ptr<Animal> a, virtual_ptr<Animal> b) {
    encounter(os, a, b);
}
