// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/openmethod.hpp>

using boost::openmethod::virtual_;

struct Animal {};
struct Dog : Animal {};

// Deliberately omit BOOST_OPENMETHOD(speak, ...) to trigger the static_assert.

BOOST_OPENMETHOD_CLASSES(Animal, Dog);

BOOST_OPENMETHOD_OVERRIDE(speak, (virtual_<Dog&>), void) {
}

int main() {
    return 0;
}
