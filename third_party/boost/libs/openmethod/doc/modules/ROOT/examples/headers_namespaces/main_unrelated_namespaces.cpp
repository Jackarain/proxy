#include <iostream>
#include <boost/openmethod.hpp>
#include <boost/openmethod/initialize.hpp>

#include "animal.hpp"
#include "cat.hpp"
#include "dog.hpp"

using animals::Animal;

namespace app_specific_behavior {

BOOST_OPENMETHOD(
    meet, (std::ostream&, virtual_ptr<Animal>, virtual_ptr<Animal>), void);

}

using app_specific_behavior::BOOST_OPENMETHOD_GUIDE(meet);

BOOST_OPENMETHOD_OVERRIDE(
    meet, (std::ostream & os, virtual_ptr<Animal>, virtual_ptr<Animal>), void) {
    os << "ignore";
}
