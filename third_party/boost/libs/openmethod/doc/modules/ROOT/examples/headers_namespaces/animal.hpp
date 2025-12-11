// animal.hpp

#ifndef ANIMAL_HPP
#define ANIMAL_HPP

#include <boost/openmethod.hpp>
#include <string>

namespace animals {

struct Animal {
    Animal(std::string name) : name(name) {
    }
    std::string name;
    virtual ~Animal() = default;
};

BOOST_OPENMETHOD(
    poke, (std::ostream&, boost::openmethod::virtual_ptr<Animal>), void);

} // namespace animals

#endif // ANIMAL_HPP
