// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_TEST_MODULE openmethod
#include <boost/test/unit_test.hpp>

#include <boost/openmethod.hpp>
#include <boost/openmethod/policies/throw_error_handler.hpp>
#include <boost/openmethod/initialize.hpp>

#include "test_util.hpp"

using namespace boost::openmethod;

struct Animal {
    virtual ~Animal() {
    }
};

struct Dog : Animal {};
struct Bulldog : Dog {};

namespace TEST_NS {

struct registry : test_registry_<__COUNTER__>::with<
                      policies::runtime_checks, policies::throw_error_handler> {
};

BOOST_OPENMETHOD(poke, (boost::openmethod::virtual_<Animal&>), void, registry);

BOOST_OPENMETHOD_OVERRIDE(poke, (Dog&), void) {
}

BOOST_OPENMETHOD_CLASSES(Animal, registry);

BOOST_AUTO_TEST_CASE(unknown_class_overrider) {
    BOOST_CHECK_THROW(initialize<registry>(), missing_class);
}

} // namespace TEST_NS

namespace TEST_NS {

struct registry : test_registry_<__COUNTER__>::with<
                      policies::runtime_checks, policies::throw_error_handler> {
};

BOOST_OPENMETHOD(poke, (boost::openmethod::virtual_<Animal&>), void, registry);

BOOST_OPENMETHOD_OVERRIDE(poke, (Dog&), void) {
}

BOOST_OPENMETHOD_CLASSES(Animal, registry);
BOOST_OPENMETHOD_CLASSES(Dog, registry); // missing base class

BOOST_AUTO_TEST_CASE(missing_base_class) {
    BOOST_CHECK_THROW(initialize<registry>(), missing_base);
}

} // namespace TEST_NS

namespace TEST_NS {

struct registry : test_registry_<__COUNTER__>::with<
                      policies::runtime_checks, policies::throw_error_handler> {
};

BOOST_OPENMETHOD(poke, (boost::openmethod::virtual_<Animal&>), void, registry);

BOOST_OPENMETHOD_OVERRIDE(poke, (Dog&), void) {
}

BOOST_OPENMETHOD_CLASSES(Animal, Dog, registry);

BOOST_AUTO_TEST_CASE(unknown_class_call) {
    initialize<registry>();
    Bulldog hector;
    BOOST_CHECK_THROW(poke(hector), missing_class);
}

} // namespace TEST_NS
