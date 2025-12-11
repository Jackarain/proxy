// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef _MSC_VER
#pragma warning(disable : 4312)
#endif

struct Animal {
    Animal(unsigned type) : type(type) {
    }

    virtual ~Animal() = default;

    unsigned type;
    static constexpr unsigned static_type = 1;
};

struct Cat : Animal {
    Cat() : Animal(static_type) {
    }

    static constexpr unsigned static_type = 2;
};

struct Dog : Animal {
    Dog() : Animal(static_type) {
    }

    static constexpr unsigned static_type = 3;
};

#include <boost/openmethod/preamble.hpp>
#include <boost/openmethod/policies/vptr_vector.hpp>

// tag::policy[]
namespace bom = boost::openmethod;

struct custom_rtti : bom::policies::rtti {
    template<class Registry>
    struct fn : bom::policies::rtti::defaults {
        template<class T>
        static constexpr bool is_polymorphic = std::is_base_of_v<Animal, T>;

        template<typename T>
        static auto static_type() -> bom::type_id {
            if constexpr (is_polymorphic<T>) {
                return reinterpret_cast<bom::type_id>(T::static_type);
            } else {
                return nullptr;
            }
        }

        template<typename T>
        static auto dynamic_type(const T& obj) -> bom::type_id {
            if constexpr (is_polymorphic<T>) {
                return reinterpret_cast<bom::type_id>(obj.type);
            } else {
                return nullptr;
            }
        }
    };
};
// end::policy[]

// tag::registry[]
struct custom_registry
    : bom::registry<custom_rtti, bom::policies::vptr_vector> {};

#define BOOST_OPENMETHOD_DEFAULT_REGISTRY custom_registry
// end::registry[]

// tag::example[]
#include <iostream>

#include <boost/openmethod.hpp>
#include <boost/openmethod/initialize.hpp>

using boost::openmethod::virtual_ptr;

BOOST_OPENMETHOD(poke, (std::ostream&, virtual_ptr<Animal>), void);

BOOST_OPENMETHOD_OVERRIDE(
    poke, (std::ostream & os, virtual_ptr<Cat> /*cat*/), void) {
    os << "hiss";
}

BOOST_OPENMETHOD_OVERRIDE(
    poke, (std::ostream & os, virtual_ptr<Dog> /*dog*/), void) {
    os << "bark";
}

BOOST_OPENMETHOD_CLASSES(Animal, Cat, Dog);

auto main() -> int {
    boost::openmethod::initialize();

    std::unique_ptr<Animal> a(new Cat);
    std::unique_ptr<Animal> b(new Dog);

    poke(std::cout, *a); // prints "hiss"
    std::cout << "\n";

    poke(std::cout, *b); // prints "bark"
    std::cout << "\n";

    return 0;
}
// end::example[]
