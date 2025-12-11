// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// clang-format off

#include <iostream>
#include <string>
#include <boost/openmethod.hpp>

using boost::openmethod::virtual_ptr;

#ifdef FRIEND_ALL

// tag::friend_all[]

namespace pets {
template<typename> struct BOOST_OPENMETHOD_OVERRIDERS(poke);
}

namespace core {
class Animal {
// end::friend_all[]

// tag::friend_all[]
  public:
    Animal(std::string name) : name(name) {
    }
    virtual ~Animal() = default;
// ...
  private:
    std::string name;

    template<typename> friend struct BOOST_OPENMETHOD_OVERRIDERS(pets::poke);
};

BOOST_OPENMETHOD(poke, (std::ostream&, virtual_ptr<Animal>), void);
}
#else

// tag::friend[]
namespace pets {
struct Cat;
struct Dog;
template<typename...> struct BOOST_OPENMETHOD_OVERRIDERS(poke);
} // namespace pets

namespace core {
class Animal {
  public:
    Animal(std::string name) : name(name) {
    }
    virtual ~Animal() = default;
// end::friend[]

// tag::friend[]
// ...
  private:
    std::string name;

    friend struct BOOST_OPENMETHOD_OVERRIDERS(pets::poke)<
        void(std::ostream&, virtual_ptr<pets::Cat>)>;
    friend struct BOOST_OPENMETHOD_OVERRIDERS(pets::poke)<
        void(std::ostream&, virtual_ptr<pets::Dog>)>;
};

BOOST_OPENMETHOD(poke, (std::ostream&, virtual_ptr<Animal>), void);
}

// end::friend[]
#endif

// tag::friend[]
namespace pets {

struct Cat : core::Animal {
    using Animal::Animal;
};

struct Dog : core::Animal {
    using Animal::Animal;
};

BOOST_OPENMETHOD_OVERRIDE(
    poke,
    (std::ostream & os, virtual_ptr<Cat> cat),
    void) {
    os << cat->name << " hisses";
}

BOOST_OPENMETHOD_OVERRIDE(
    poke, (std::ostream & os, virtual_ptr<Dog> dog), void) {
    os << dog->name << " barks";
}

BOOST_OPENMETHOD_CLASSES(core::Animal, Cat, Dog);
} // namespace pets
// end::friend[]

#include <boost/openmethod/initialize.hpp>

auto main() -> int {
    boost::openmethod::initialize();

    std::unique_ptr<core::Animal> a(new pets::Cat("Felix"));
    std::unique_ptr<core::Animal> b(new pets::Dog("Snoopy"));

    core::poke(std::cout, *a); // Felix hisses
    std::cout << ".\n";

    core::poke(std::cout, *b); // Snoopy barks
    std::cout << ".\n";

    return 0;
}
