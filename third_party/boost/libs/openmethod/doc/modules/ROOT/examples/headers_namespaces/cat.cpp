// cat.cpp

#include <iostream>
#include <boost/openmethod.hpp>

#include "cat.hpp"

using boost::openmethod::virtual_ptr;

namespace felines {

BOOST_OPENMETHOD_CLASSES(animals::Animal, Cat, Cheetah);

BOOST_OPENMETHOD_OVERRIDE(
    poke, (std::ostream & os, virtual_ptr<Cat> cat), void) {
    os << cat->name << " hisses";
}

BOOST_OPENMETHOD_OVERRIDE(
    poke, (std::ostream & os, virtual_ptr<Cheetah> cat), void) {
    BOOST_OPENMETHOD_OVERRIDER(
        poke, (std::ostream & os, virtual_ptr<Cat> dog), void)::fn(os, cat);
    os << " and runs away";
}

} // namespace felines
