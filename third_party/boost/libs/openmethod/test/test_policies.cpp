// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <type_traits>

#define BOOST_TEST_MODULE policies
#include <boost/test/unit_test.hpp>

#include <boost/openmethod/default_registry.hpp>

#include "test_util.hpp"

using namespace boost::openmethod;
using namespace boost::openmethod::detail;
namespace mp11 = boost::mp11;

using namespace policies;

struct base {
    using category = base;
};

struct derived : base {
    template<class>
    struct fn {};
};

static_assert(std::is_same_v<
              registry<derived>::policy<base>, derived::fn<registry<derived>>>);

static_assert(detail::is_registry<default_registry>);

struct not_a_policy {};
static_assert(!detail::is_registry<not_a_policy>);

struct registry1 : default_registry::with<unique<registry1>> {};
struct registry2 : default_registry::with<unique<registry2>> {};

struct foo {
    using category = foo;
};

struct foo1 : foo {};
struct foo2 : foo {};

struct bar {
    using category = bar;
};

struct bar1 : bar {};
struct bar2 : bar {};

static_assert(std::is_same_v<registry<>::with<foo1>, registry<foo1>>);
static_assert(std::is_same_v<registry<foo1>::with<foo2>, registry<foo2>>);
static_assert(std::is_same_v<
              registry<foo1, bar1>::with<foo2, bar2>, registry<foo2, bar2>>);
static_assert(std::is_same_v<
              registry<foo1, bar1>::with<bar2, foo2>, registry<foo2, bar2>>);
static_assert(
    std::is_same_v<registry<foo1, bar1>::without<bar>, registry<foo1>>);

BOOST_AUTO_TEST_CASE(test_registry) {
    using namespace policies;

    // BOOST_TEST(&registry2::methods != &registry1::methods);
    // BOOST_TEST(&registry2::classes != &registry1::classes);
    BOOST_TEST(&registry2::static_vptr<void> != &registry1::static_vptr<void>);
    // BOOST_TEST(&registry2::dispatch_data != &registry1::dispatch_data);
}
