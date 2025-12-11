// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// tag::content[]
// extensions.cpp
#include "animals.hpp"

using namespace boost::openmethod::aliases;

BOOST_OPENMETHOD_OVERRIDE(
    meet, (virtual_ptr<Herbivore>, virtual_ptr<Carnivore>), std::string) {
    return "run";
}

BOOST_OPENMETHOD_OVERRIDE(
    meet, (virtual_ptr<Carnivore>, virtual_ptr<Herbivore>), std::string) {
    return "hunt";
}

struct Tiger : Carnivore {};

BOOST_OPENMETHOD_CLASSES(Tiger, Carnivore);

extern "C" {
#ifdef _WIN32
__declspec(dllexport)
#endif
auto make_tiger() -> Animal* {
    return new Tiger;
}
}
// end::content[]
