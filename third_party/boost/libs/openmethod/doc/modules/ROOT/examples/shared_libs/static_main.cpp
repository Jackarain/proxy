// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// tag::content[]
// static_main.cpp

#include "animals.hpp"
#include <boost/openmethod/initialize.hpp>
#include <iostream>

using namespace boost::openmethod::aliases;

struct Cow : Herbivore {};
struct Wolf : Carnivore {};

BOOST_OPENMETHOD_CLASSES(Animal, Herbivore, Cow, Wolf, Carnivore);

BOOST_OPENMETHOD_OVERRIDE(
    meet, (virtual_ptr<Animal>, virtual_ptr<Animal>), std::string) {
    return "greet";
}

extern "C" {
#ifdef _WIN32
__declspec(dllimport)
#endif
auto make_tiger() -> Animal*;
}

auto main() -> int {
    boost::openmethod::initialize();

    std::unique_ptr<Animal> gracie(new Cow());
    std::unique_ptr<Animal> willy(new Wolf());

    std::cout << "cow meets wolf -> " << meet(*gracie, *willy) << "\n"; // run
    std::cout << "wolf meets cow -> " << meet(*willy, *gracie) << "\n"; // hunt

    std::unique_ptr<Animal> hobbes(make_tiger());
    std::cout << "cow meets tiger -> " << meet(*gracie, *hobbes) << "\n"; // run

    return 0;
}
// end::content[]
