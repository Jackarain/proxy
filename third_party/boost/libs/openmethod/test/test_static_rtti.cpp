// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_TEST_MODULE openmethod
#include <boost/test/unit_test.hpp>

#include <boost/openmethod/default_registry.hpp>
#include <boost/openmethod/policies/static_rtti.hpp>

struct static_registry
    : boost::openmethod::registry<boost::openmethod::policies::static_rtti> {};

#define BOOST_OPENMETHOD_DEFAULT_REGISTRY static_registry

#include <boost/openmethod.hpp>
#include <boost/openmethod/interop/std_unique_ptr.hpp>
#include <boost/openmethod/initialize.hpp>

struct Animal {};

struct Dog : Animal {};

struct Cat : Animal {};

using namespace boost::openmethod::aliases;

BOOST_OPENMETHOD_CLASSES(Animal, Dog, Cat);

BOOST_OPENMETHOD(poke, (virtual_ptr<Animal>, std::ostream&), void);

BOOST_OPENMETHOD_OVERRIDE(poke, (virtual_ptr<Dog>, std::ostream& os), void) {
    os << "bark";
}

BOOST_OPENMETHOD_OVERRIDE(poke, (virtual_ptr<Cat>, std::ostream& os), void) {
    os << "hiss";
}

BOOST_AUTO_TEST_CASE(static_rtti) {
    boost::openmethod::initialize();

    auto a = make_unique_virtual<Cat>();
    auto b = make_unique_virtual<Dog>();

    {
        std::stringstream os;
        poke(a, os);
        BOOST_TEST(os.str() == "hiss");
    }
    {
        std::stringstream os;
        poke(b, os);
        BOOST_TEST(os.str() == "bark");
    }
}
