// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// clang-format off

// tag::domain_classes[]
#include <string>

struct Animal {
    Animal(std::string name) : name(name) {}
    std::string name;
    virtual ~Animal() = default;
};

struct Cat : Animal {
    using Animal::Animal;
};

struct Dog : Animal {
    using Animal::Animal;
};

struct Bulldog : Dog {
    using Dog::Dog;
};
// end::domain_classes[]

// tag::method[]
#include <iostream>
#include <boost/openmethod.hpp>

using boost::openmethod::virtual_ptr;

BOOST_OPENMETHOD(
    poke,                                       // method name
    (std::ostream&, virtual_ptr<Animal>),       // method signature
    void);                                      // return type
// end::method[]

// tag::overriders[]
BOOST_OPENMETHOD_OVERRIDE(
    poke,                                       // method name
    (std::ostream & os, virtual_ptr<Cat> cat),  // overrider signature
    void) {                                     // return type
    os << cat->name << " hisses";               // overrider body
}

BOOST_OPENMETHOD_OVERRIDE(poke, (std::ostream & os, virtual_ptr<Dog> dog), void) {
    os << dog->name << " barks";
}
// end::overriders[]

// tag::next[]
BOOST_OPENMETHOD_OVERRIDE(
    poke, (std::ostream & os, virtual_ptr<Bulldog> dog), void) {
    next(os, dog);                              // call base overrider
    os << " and bites back";
}
// end::next[]

// tag::classes[]
BOOST_OPENMETHOD_CLASSES(Animal, Cat, Dog, Bulldog);
// end::classes[]

// tag::multi[]
BOOST_OPENMETHOD(
    encounter,
    (std::ostream&, virtual_ptr<Animal>, virtual_ptr<Animal>), void);

// 'encounter' catch-all implementation.
BOOST_OPENMETHOD_OVERRIDE(
    encounter,
    (std::ostream & os, virtual_ptr<Animal> a, virtual_ptr<Animal> b), void) {
    os << a->name << " and " << b->name << " ignore each other";
}

// Add definitions for specific pairs of animals.
BOOST_OPENMETHOD_OVERRIDE(
    encounter,
    (std::ostream & os, virtual_ptr<Dog> /*dog1*/, virtual_ptr<Dog> /*dog2*/), void) {
    os << "Both wag tails";
}

BOOST_OPENMETHOD_OVERRIDE(
    encounter, (std::ostream & os, virtual_ptr<Dog> dog, virtual_ptr<Cat> cat),
    void) {
    os << dog->name << " chases " << cat->name;
}

BOOST_OPENMETHOD_OVERRIDE(
    encounter, (std::ostream & os, virtual_ptr<Cat> cat, virtual_ptr<Dog> dog),
    void) {
    os << cat->name << " runs away from " << dog->name;
}
// end::multi[]

// tag::main[]
#include <boost/openmethod/initialize.hpp>
    // only needed in the file that calls boost::openmethod::initialize()

auto main() -> int {
    boost::openmethod::initialize();
    // ...
    // end::main[]

    // tag::call[]
    std::unique_ptr<Animal> felix(new Cat("Felix"));
    std::unique_ptr<Animal> snoopy(new Dog("Snoopy"));
    std::unique_ptr<Animal> hector(new Bulldog("Hector"));

    poke(std::cout, *felix); // Felix hisses
    std::cout << ".\n";

    poke(std::cout, *snoopy); // Snoopy barks
    std::cout << ".\n";

    poke(std::cout, *hector); // Hector barks and bites
    std::cout << ".\n";
    // end::call[]

    // tag::multi_call[]
    // cat and dog
    encounter(std::cout, *felix, *snoopy); // Felix runs away from Snoopy
    std::cout << ".\n";

    // dog and cat
    encounter(std::cout, *snoopy, *felix); // Snoopy chases Felix
    std::cout << ".\n";

    // dog and dog
    encounter(std::cout, *snoopy, *hector); // Both wag tails
    std::cout << ".\n";

    // cat and cat
    std::unique_ptr<Animal> tom(new Cat("Tom"));
    encounter(std::cout, *felix, *tom); // Felix and Tom ignore each other
    std::cout << ".\n";
    // end::multi_call[]


    return 0;
    // tag::main[]
}
// end::main[]

// tag::call_poke_via_virtual_ptr[]
void call_poke_via_virtual_ptr(std::ostream& os, virtual_ptr<Animal> a) {
    poke(os, a);
}
// end::call_poke_via_virtual_ptr[]

// tag::call_poke_via_final_virtual_ptr[]
void call_poke_via_final_virtual_ptr(std::ostream& os, Cat& cat) {
    poke(os, boost::openmethod::final_virtual_ptr(cat));
}
// end::call_poke_via_final_virtual_ptr[]

// tag::call_poke_via_ref[]
void call_poke_via_ref(std::ostream& os, Animal& a) {
    poke(os, a);
}
// end::call_poke_via_ref[]

void call_encounter(
    std::ostream& os, virtual_ptr<Animal> a, virtual_ptr<Animal> b) {
    encounter(os, a, b);
}
