// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <any>

#include <boost/openmethod.hpp>
#include <boost/openmethod/interop/boost_intrusive_ptr.hpp>
#include <boost/openmethod/initialize.hpp>
#include <boost/smart_ptr/intrusive_ptr.hpp>
#include <boost/smart_ptr/intrusive_ref_counter.hpp>

#define BOOST_TEST_MODULE openmethod
#include <boost/test/unit_test.hpp>

using namespace boost::openmethod;

#define MAKE_CLASSES()                                                         \
    struct Animal : boost::intrusive_ref_counter<Animal> {                     \
        explicit Animal(std::string str) {                                     \
            name = std::move(str);                                             \
        }                                                                      \
                                                                               \
        Animal(const Animal&) = delete;                                        \
        Animal(Animal&&) = default;                                            \
        virtual ~Animal() = default;                                           \
                                                                               \
        std::string name;                                                      \
    };                                                                         \
                                                                               \
    struct Dog : Animal {                                                      \
        using Animal::Animal;                                                  \
    };                                                                         \
                                                                               \
    struct Cat : virtual Animal {                                              \
        using Animal::Animal;                                                  \
    };                                                                         \
                                                                               \
    BOOST_OPENMETHOD_CLASSES(Animal, Dog, Cat);

namespace BOOST_OPENMETHOD_GENSYM {

// -----------------------------------------------------------------------------
// pass virtual args by shared_ptr by value

MAKE_CLASSES();

BOOST_OPENMETHOD(
    name, (virtual_<boost::intrusive_ptr<const Animal>>), std::string);

BOOST_OPENMETHOD_OVERRIDE(
    name, (boost::intrusive_ptr<const Cat> cat), std::string) {
    return cat->name + " the cat";
}

BOOST_OPENMETHOD_OVERRIDE(
    name, (boost::intrusive_ptr<const Dog> dog), std::string) {
    return dog->name + " the dog";
}

BOOST_AUTO_TEST_CASE(intrusive_ptr_by_value) {
    initialize();

    auto spot = boost::intrusive_ptr<const Dog>(new Dog("Spot"));
    BOOST_TEST(name(spot) == "Spot the dog");

    auto felix = boost::intrusive_ptr<const Cat>(new Cat("Felix"));
    BOOST_TEST(name(felix) == "Felix the cat");
}
} // namespace BOOST_OPENMETHOD_GENSYM

namespace BOOST_OPENMETHOD_GENSYM {

// -----------------------------------------------------------------------------
// pass virtual args by shared_ptr by const&

MAKE_CLASSES();

BOOST_OPENMETHOD(
    name, (virtual_<const boost::intrusive_ptr<const Animal>&>), std::string);

BOOST_OPENMETHOD_OVERRIDE(
    name, (const boost::intrusive_ptr<const Cat>& cat), std::string) {
    return cat->name + " the cat";
}

BOOST_OPENMETHOD_OVERRIDE(
    name, (const boost::intrusive_ptr<const Dog>& dog), std::string) {
    return dog->name + " the dog";
}

BOOST_AUTO_TEST_CASE(intrusive_ptr_by_const_ref) {
    initialize();

    auto spot = boost::intrusive_ptr<const Dog>(new Dog("Spot"));
    BOOST_TEST(name(spot) == "Spot the dog");

    auto felix = boost::intrusive_ptr<const Cat>(new Cat("Felix"));
    BOOST_TEST(name(felix) == "Felix the cat");
}
} // namespace BOOST_OPENMETHOD_GENSYM

namespace BOOST_OPENMETHOD_GENSYM {

// -----------------------------------------------------------------------------
// virtual_ptr<intrusive_ptr> by value

MAKE_CLASSES();

BOOST_OPENMETHOD(
    name, (boost_intrusive_virtual_ptr<const Animal>), std::string);

BOOST_OPENMETHOD_OVERRIDE(
    name, (boost_intrusive_virtual_ptr<const Cat> cat), std::string) {
    return cat->name + " the cat";
}

BOOST_OPENMETHOD_OVERRIDE(
    name, (boost_intrusive_virtual_ptr<const Dog> dog), std::string) {
    return dog->name + " the dog";
}

BOOST_AUTO_TEST_CASE(intrusive_virtual_ptr_by_value) {
    initialize();

    auto spot = make_boost_intrusive_virtual<const Dog>("Spot");
    BOOST_TEST(name(spot) == "Spot the dog");

    auto felix = make_boost_intrusive_virtual<const Cat>("Felix");
    BOOST_TEST(name(felix) == "Felix the cat");
}
} // namespace BOOST_OPENMETHOD_GENSYM

namespace BOOST_OPENMETHOD_GENSYM {

// -----------------------------------------------------------------------------
// virtual_ptr<intrusive_ptr> by const&

MAKE_CLASSES();

BOOST_OPENMETHOD(
    name, (const boost_intrusive_virtual_ptr<const Animal>&), std::string);

BOOST_OPENMETHOD_OVERRIDE(
    name, (const boost_intrusive_virtual_ptr<const Cat>& cat), std::string) {
    return cat->name + " the cat";
}

BOOST_OPENMETHOD_OVERRIDE(
    name, (const boost_intrusive_virtual_ptr<const Dog>& dog), std::string) {
    return dog->name + " the dog";
}

BOOST_AUTO_TEST_CASE(intrusive_virtual_ptr_by_const_ref) {
    initialize();

    auto spot = make_boost_intrusive_virtual<const Dog>("Spot");
    BOOST_TEST(name(spot) == "Spot the dog");

    auto felix = make_boost_intrusive_virtual<const Cat>("Felix");
    BOOST_TEST(name(felix) == "Felix the cat");
}
} // namespace BOOST_OPENMETHOD_GENSYM
