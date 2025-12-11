// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/openmethod.hpp>

using namespace boost::openmethod;

class Animal {
  public:
    virtual ~Animal() = default;
};

class Cat : private Animal {
  public:
    Animal& as_animal() {
        return *this;
    }
};

struct poke;

auto poke_cat(virtual_ptr<Cat>) -> void {
}

BOOST_OPENMETHOD_REGISTER(
    method<poke, void(virtual_ptr<Animal>)>::override<poke_cat>);

int main() {
}
