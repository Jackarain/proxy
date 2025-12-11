// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>

#ifdef BOOST_OPENMETHOD_ENABLE_RUNTIME_CHECKS
#undef BOOST_OPENMETHOD_ENABLE_RUNTIME_CHECKS
#else
#define BOOST_OPENMETHOD_ENABLE_RUNTIME_CHECKS
#endif

#include "lib.hpp"

using namespace boost::openmethod;

BOOST_OPENMETHOD_CLASSES(Animal, Cat);

BOOST_OPENMETHOD_OVERRIDE(poke, (virtual_ptr<Cat>, std::ostream& os), void) {
    os << "hiss";
}
