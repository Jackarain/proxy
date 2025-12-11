// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/openmethod.hpp>

using boost::openmethod::virtual_ptr;

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

BOOST_OPENMETHOD(poke, (virtual_ptr<Animal&>), void);

BOOST_OPENMETHOD_OVERRIDE(poke, (virtual_ptr<Cat>), void) {
}

int main() {
    Cat felix;
    poke(felix.as_animal());
    return 0;
}
