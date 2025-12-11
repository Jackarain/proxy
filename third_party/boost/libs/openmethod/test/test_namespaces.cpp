// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

namespace interfaces {
class Animal {
  public:
    virtual ~Animal() {
    }
};
} // namespace interfaces

namespace canis {
class Dog : public interfaces::Animal {};
class Bulldog : public Dog {};
} // namespace canis

namespace felis {
class Cat : public interfaces::Animal {};
} // namespace felis

namespace delphinus {
class Dolphin : public interfaces::Animal {};
} // namespace delphinus

#include <string>

#include <boost/openmethod.hpp>
#include <boost/openmethod/initialize.hpp>

using boost::openmethod::virtual_;

BOOST_OPENMETHOD_CLASSES(
    interfaces::Animal, canis::Dog, canis::Bulldog, felis::Cat,
    delphinus::Dolphin);

// open method with single virtual argument <=> virtual function "from outside"
BOOST_OPENMETHOD(poke, (virtual_<interfaces::Animal&>), std::string);

namespace canis {
// implement 'poke' for dogs
BOOST_OPENMETHOD_OVERRIDE(poke, (Dog&), std::string) {
    return "bark";
}

// implement 'poke' for bulldogs
BOOST_OPENMETHOD_OVERRIDE(poke, (Bulldog & dog), std::string) {
    return next(dog) + " and bite";
}
} // namespace canis

// multi-method
BOOST_OPENMETHOD(
    meet, (virtual_<interfaces::Animal&>, virtual_<interfaces::Animal&>),
    std::string);

// 'meet' catch-all implementation
BOOST_OPENMETHOD_OVERRIDE(
    meet, (interfaces::Animal&, interfaces::Animal&), std::string) {
    return "ignore";
}

BOOST_OPENMETHOD_OVERRIDE(meet, (canis::Dog&, canis::Dog&), std::string) {
    return "wag tail";
}

BOOST_OPENMETHOD_OVERRIDE(meet, (canis::Dog&, felis::Cat&), std::string) {
    return "chase";
}

BOOST_OPENMETHOD_OVERRIDE(meet, (felis::Cat&, canis::Dog&), std::string) {
    return "run";
}

// -----------------------------------------------------------------------------
// main

#include <iostream>
#include <memory>

auto main() -> int {
    boost::openmethod::initialize();

    std::unique_ptr<interfaces::Animal> hector =
                                            std::make_unique<canis::Bulldog>(),
                                        snoopy = std::make_unique<canis::Dog>();

    std::cout << "poke snoopy: " << poke(*snoopy) << "\n"; // bark
    std::cout << "poke hector: " << poke(*hector) << "\n"; // bark and bite

    std::unique_ptr<interfaces::Animal>
        sylvester = std::make_unique<felis::Cat>(),
        flipper = std::make_unique<delphinus::Dolphin>();

    std::cout << "hector meets sylvester: " << meet(*hector, *sylvester)
              << "\n"; // chase
    std::cout << "sylvester meets hector: " << meet(*sylvester, *hector)
              << "\n"; // run
    std::cout << "hector meets snoopy: " << meet(*hector, *snoopy)
              << "\n"; // wag tail
    std::cout << "hector meets flipper: " << meet(*hector, *flipper)
              << "\n"; // ignore
}
