// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>

#include <boost/openmethod.hpp>
#include <boost/openmethod/inplace_vptr.hpp>
#include <boost/openmethod/initialize.hpp>

using boost::openmethod::virtual_;

// clang-format off

namespace just_virtual {

// tag::virtual_parameter[]
struct Animal {
    virtual ~Animal() = default;
};

struct Cat : Animal {};

using boost::openmethod::virtual_;

BOOST_OPENMETHOD(poke, (std::ostream&, virtual_<Animal&>), void);

BOOST_OPENMETHOD_OVERRIDE(poke, (std::ostream & os, Cat& /*cat*/), void) {
    os << "hiss";
}

BOOST_OPENMETHOD_CLASSES(Animal, Cat);
// end::virtual_parameter[]
} // namespace just_virtual

namespace virtual_intrusive {

// tag::virtual_intrusive[]

class Animal {
  protected:
    boost::openmethod::vptr_type vptr;
    friend auto boost_openmethod_vptr(const Animal& a, void*) {
        return a.vptr;
    }

  public:
    Animal() {
        vptr = boost::openmethod::default_registry::static_vptr<Animal>;
    }
};

class Cat : public Animal {
  public:
    Cat() {
        vptr = boost::openmethod::default_registry::static_vptr<Cat>;
    }
};

BOOST_OPENMETHOD(poke, (std::ostream&, virtual_<Animal&>), void);

BOOST_OPENMETHOD_OVERRIDE(poke, (std::ostream & os, Cat& /*cat*/), void) {
    os << "hiss\n";
}

BOOST_OPENMETHOD_CLASSES(Animal, Cat);
// end::virtual_intrusive[]
} // namespace virtual_intrusive

namespace inplace_vptr {

// tag::inplace_vptr[]

#include <boost/openmethod/inplace_vptr.hpp>

class Animal : public boost::openmethod::inplace_vptr_base<Animal> {
};

class Cat : public Animal, public boost::openmethod::inplace_vptr_derived<Cat, Animal> {
};

BOOST_OPENMETHOD(poke, (std::ostream&, virtual_<Animal&>), void);

BOOST_OPENMETHOD_OVERRIDE(poke, (std::ostream & os, Cat& /*cat*/), void) {
    os << "hiss\n";
}
// end::inplace_vptr[]

} // namespace inplace_vptr

auto main() -> int {
    boost::openmethod::initialize();

    {
        using namespace just_virtual;
        Cat cat;
        poke(std::cout, cat); // hiss
    }

    {
        using namespace virtual_intrusive;
        Cat cat;
        poke(std::cout, cat); // hiss
    }

    {
        using namespace inplace_vptr;
        Cat cat;
        poke(std::cout, cat); // hiss
    }
}
