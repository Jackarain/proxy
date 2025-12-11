// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef TEST_VIRTUAL_PTR_VALUE_SEMANTICS_HPP
#define TEST_VIRTUAL_PTR_VALUE_SEMANTICS_HPP

#include <boost/openmethod.hpp>
#include <boost/openmethod/policies/vptr_map.hpp>
#include <boost/openmethod/initialize.hpp>
#include <boost/openmethod/interop/std_unique_ptr.hpp>

#include "test_util.hpp"

#include <utility>

using namespace boost::openmethod;
using namespace policies;
using namespace detail;

struct Animal {
    virtual ~Animal() {
    }

    Animal() = default;
    Animal(const Animal&) = delete;
};

struct Cat : virtual Animal {};

struct Dog : Animal {};

template<class Left, class Right>
constexpr bool construct_assign_ok =
    std::is_constructible_v<Left, Right> && std::is_assignable_v<Left, Right>;

struct NonPolymorphic {};

template<class Registry>
void init_test() {
    BOOST_OPENMETHOD_REGISTER(use_classes<Animal, Cat, Dog, Registry>);
    struct id;
    // without following line, no methods, no v-tables
    (void)&method<id, auto(virtual_ptr<Animal, Registry>)->void, Registry>::fn;
    initialize<Registry>();
}

struct direct_vector : test_registry_<__COUNTER__> {};

struct indirect_vector : test_registry_<__COUNTER__>::with<indirect_vptr> {};

struct direct_map
    : test_registry_<__COUNTER__>::with<vptr_map<>>::without<type_hash> {};

struct indirect_map : direct_map::with<indirect_vptr> {};

using test_policies = boost::mp11::mp_list<
    direct_vector, indirect_vector, direct_map, indirect_map>;

using test_classes = boost::mp11::mp_list<Dog, Cat>;

template<
    template<class... Class> class smart_ptr,
    template<class... Class> class other_smart_ptr, class Registry>
struct check_illegal_smart_ops {

    // a virtual_ptr cannot be constructed from a smart_ptr to a different class
    static_assert(!std::is_constructible_v<
                  virtual_ptr<smart_ptr<Cat>, Registry>, smart_ptr<Dog>>);

    // a virtual_ptr cannot be constructed from const smart_ptr
    static_assert(
        !std::is_constructible_v<
            virtual_ptr<smart_ptr<Animal>, Registry>, smart_ptr<const Animal>>);

    // a smart virtual_ptr cannot be constructed from a plain reference or
    // pointer
    static_assert(!std::is_constructible_v<
                  virtual_ptr<smart_ptr<Animal>, Registry>, Animal&>);
    static_assert(!std::is_constructible_v<
                  virtual_ptr<smart_ptr<Animal>, Registry>, Animal*>);

    static_assert(!std::is_constructible_v<
                  smart_ptr<Animal>, const other_smart_ptr<Animal>&>);
    // smart_ptr<Animal> p{other_smart_ptr<Animal>()};

    static_assert(!std::is_constructible_v<
                  virtual_ptr<smart_ptr<Animal>, Registry>,
                  virtual_ptr<Animal, Registry>>);

    // ---------------------
    // test other properties

    static_assert(IsSmartPtr<smart_ptr<Animal>, Registry>);
    static_assert(IsSmartPtr<smart_ptr<const Animal>, Registry>);

    static_assert(
        std::is_same_v<
            typename virtual_ptr<smart_ptr<Animal>, Registry>::element_type,
            Animal>);

    static_assert(
        std::is_same_v<
            decltype(std::declval<virtual_ptr<smart_ptr<Animal>, Registry>>()
                         .get()),
            Animal*>);

    static_assert(
        std::is_same_v<
            decltype(*std::declval<virtual_ptr<smart_ptr<Animal>, Registry>>()),
            Animal&>);

    static_assert(
        std::is_same_v<
            decltype(std::declval<virtual_ptr<smart_ptr<Animal>, Registry>>()
                         .pointer()),
            const smart_ptr<Animal>&>);

    static_assert(
        std::is_same_v<
            decltype(*std::declval<virtual_ptr<smart_ptr<Animal>, Registry>>()),
            Animal&>);
};

#endif // TEST_VIRTUAL_PTR_VALUE_SEMANTICS_HPP
