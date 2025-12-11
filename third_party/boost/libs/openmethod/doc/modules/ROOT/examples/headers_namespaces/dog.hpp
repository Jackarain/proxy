#ifndef CANINES_HPP
#define CANINES_HPP

#include <iosfwd>
#include <boost/openmethod.hpp>

#include "animal.hpp"

namespace canines {

struct Dog : animals::Animal {
    using Animal::Animal;
};

BOOST_OPENMETHOD_DECLARE_OVERRIDER(
    poke, (std::ostream & os, boost::openmethod::virtual_ptr<Dog> dog), void);

} // namespace canines

#endif // CANINES_HPP
