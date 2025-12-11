#include <iostream>
#include <boost/openmethod.hpp>
#include <boost/openmethod/initialize.hpp>

#include "animal.hpp"
#include "cat.hpp"
#include "dog.hpp"

using namespace animals;
using namespace canines;
using namespace felines;

struct Bulldog : Dog {
    using Dog::Dog;
};

BOOST_OPENMETHOD_CLASSES(Dog, Bulldog);

BOOST_OPENMETHOD_OVERRIDE(
    poke, (std::ostream & os, virtual_ptr<Bulldog> dog), void) {
    next(os, dog);
    os << " and bites back";
}

auto main() -> int {
    boost::openmethod::initialize();

    std::unique_ptr<animals::Animal> felix(new Cat("Felix"));
    std::unique_ptr<animals::Animal> snoopy(new Dog("Snoopy"));
    std::unique_ptr<animals::Animal> hector(new Bulldog("Hector"));

    poke(std::cout, *felix); // Felix hisses
    std::cout << ".\n";

    poke(std::cout, *snoopy); // Snoopy barks
    std::cout << ".\n";

    poke(std::cout, *hector); // Hector barks and bites
    std::cout << ".\n";

    return 0;
}
