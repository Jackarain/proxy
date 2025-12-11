// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/openmethod.hpp>

using namespace boost::openmethod;

struct Animal {
    virtual ~Animal() {
    }
};
struct Cat : Animal {};

BOOST_OPENMETHOD(poke, (const virtual_ptr<Animal>&), void);

BOOST_OPENMETHOD_OVERRIDE(poke, (virtual_ptr<Cat>), void) {
}

int main() {
    Cat felix;
    poke(felix);
    return 0;
}
