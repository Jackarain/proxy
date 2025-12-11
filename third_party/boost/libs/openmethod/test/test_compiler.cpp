// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <type_traits>

#include <boost/openmethod.hpp>
#include <boost/openmethod/initialize.hpp>

#include "test_util.hpp"

#define BOOST_TEST_MODULE compiler
#include <boost/test/unit_test.hpp>

using namespace boost::openmethod;

using class_ = detail::generic_compiler::class_;
using cc_method = detail::generic_compiler::method;
using overrider = detail::generic_compiler::overrider;

auto operator<<(std::ostream& os, const class_* cls) -> std::ostream& {
    return os
        << reinterpret_cast<const std::type_info*>(cls->type_ids[0])->name();
}

std::string empty = "{}";

template<template<typename...> class Container, typename T>
auto str(const Container<T>& container) {
    std::ostringstream os;
    os << "{";
    const char* sep = "";

    for (const auto& item : container) {
        os << sep << item;
        sep = ", ";
    }

    os << "}";

    return os.str();
}

template<typename... Ts>
auto sstr(Ts... args) {
    std::vector<class_*> vec{args...};
    std::sort(vec.begin(), vec.end());
    return str(vec);
}

template<typename T>
auto sstr(const std::unordered_set<T>& container) {
    return sstr(std::vector<T>(container.begin(), container.end()));
}

template<typename T, typename Compiler>
auto get_class(const Compiler& comp) {
    return comp.class_map.at(typeid(T));
}

/*
A   B
 \ / \
 AB   D
 |    |
 C    E
  \  /
   F
*/

struct A {
    virtual ~A() {
    }
};

struct B {
    virtual ~B() {
    }
};

struct AB : A, B {};

struct C : AB {};

struct D : B {};

struct E : D {};

struct F : C, E {};

// ============================================================================
// Test use_classes.

BOOST_AUTO_TEST_CASE(test_use_classes_linear) {
    struct Base {
        virtual ~Base() = default;
    };

    struct D1 : Base {};
    struct D2 : D1 {};
    struct D3 : D2 {};
    struct D4 : D3 {};
    struct D5 : D4 {};

    struct registry : test_registry_<__COUNTER__> {};

    BOOST_OPENMETHOD_CLASSES(Base, D1, D2, D3, registry);
    BOOST_OPENMETHOD_CLASSES(D2, D3, registry);
    BOOST_OPENMETHOD_CLASSES(D3, D4, registry);
    BOOST_OPENMETHOD_CLASSES(D4, D5, D3, registry);

    auto comp = initialize<registry>();

    auto base = get_class<Base>(comp);
    auto d1 = get_class<D1>(comp);
    auto d2 = get_class<D2>(comp);
    auto d3 = get_class<D3>(comp);
    auto d4 = get_class<D4>(comp);
    auto d5 = get_class<D5>(comp);

    BOOST_CHECK_EQUAL(sstr(base->direct_bases), empty);
    BOOST_CHECK_EQUAL(sstr(base->direct_derived), sstr(d1));
    BOOST_CHECK_EQUAL(
        sstr(base->transitive_derived), sstr(base, d1, d2, d3, d4, d5));

    BOOST_CHECK_EQUAL(sstr(d1->direct_derived), sstr(d2));
    BOOST_CHECK_EQUAL(sstr(d1->direct_bases), sstr(base));
    BOOST_CHECK_EQUAL(sstr(d1->transitive_derived), sstr(d1, d2, d3, d4, d5));

    BOOST_CHECK_EQUAL(sstr(d2->direct_derived), sstr(d3));
    BOOST_CHECK_EQUAL(sstr(d2->direct_bases), sstr(d1));
    BOOST_CHECK_EQUAL(sstr(d2->transitive_derived), sstr(d2, d3, d4, d5));

    BOOST_CHECK_EQUAL(sstr(d3->direct_derived), sstr(d4));
    BOOST_CHECK_EQUAL(sstr(d3->direct_bases), sstr(d2));
    BOOST_CHECK_EQUAL(sstr(d3->transitive_derived), sstr(d3, d4, d5));

    BOOST_CHECK_EQUAL(sstr(d4->direct_derived), sstr(d5));
    BOOST_CHECK_EQUAL(sstr(d4->direct_bases), sstr(d3));
    BOOST_CHECK_EQUAL(sstr(d4->transitive_derived), sstr(d4, d5));
}

BOOST_AUTO_TEST_CASE(test_use_classes_diamond) {
    using test_registry = test_registry_<__COUNTER__>;
    BOOST_OPENMETHOD_REGISTER(use_classes<A, B, AB, C, D, E, test_registry>);

    std::vector<class_*> actual, expected;

    auto comp = initialize<test_registry>();

    auto a = get_class<A>(comp);
    auto b = get_class<B>(comp);
    auto ab = get_class<AB>(comp);
    auto c = get_class<C>(comp);
    auto d = get_class<D>(comp);
    auto e = get_class<E>(comp);

    // -----------------------------------------------------------------------
    // A
    BOOST_REQUIRE_EQUAL(sstr(a->direct_bases), empty);
    BOOST_REQUIRE_EQUAL(sstr(a->direct_derived), sstr(ab));
    BOOST_REQUIRE_EQUAL(sstr(a->transitive_derived), sstr(a, ab, c));

    // -----------------------------------------------------------------------
    // B
    BOOST_REQUIRE_EQUAL(sstr(b->direct_bases), empty);
    BOOST_REQUIRE_EQUAL(sstr(b->direct_derived), sstr(ab, d));
    BOOST_REQUIRE_EQUAL(sstr(b->transitive_derived), sstr(b, ab, c, d, e));

    // -----------------------------------------------------------------------
    // AB
    BOOST_REQUIRE_EQUAL(sstr(ab->direct_bases), sstr(a, b));
    BOOST_REQUIRE_EQUAL(sstr(ab->direct_derived), sstr(c));
    BOOST_REQUIRE_EQUAL(sstr(ab->transitive_derived), sstr(ab, c));

    // -----------------------------------------------------------------------
    // C
    BOOST_REQUIRE_EQUAL(sstr(c->direct_bases), sstr(ab));
    BOOST_REQUIRE_EQUAL(sstr(c->direct_derived), empty);
    BOOST_REQUIRE_EQUAL(sstr(c->transitive_derived), sstr(c));

    // -----------------------------------------------------------------------
    // D
    BOOST_REQUIRE_EQUAL(sstr(d->direct_bases), sstr(b));
    BOOST_REQUIRE_EQUAL(sstr(d->direct_derived), sstr(e));
    BOOST_REQUIRE_EQUAL(sstr(d->transitive_derived), sstr(d, e));

    // -----------------------------------------------------------------------
    // E
    BOOST_REQUIRE_EQUAL(sstr(e->direct_bases), sstr(d));
    BOOST_REQUIRE_EQUAL(sstr(e->direct_derived), empty);
    BOOST_REQUIRE_EQUAL(sstr(e->transitive_derived), sstr(e));
}

/// ============================================================================
// Test assign_slots.

auto check(const detail::generic_compiler::method* method)
    -> const detail::generic_compiler::method* {
    if (method) {
        return method;
    }

    BOOST_FAIL("method not found");

    return nullptr;
}

template<int>
struct M;

#define ADD_METHOD(CLASS)                                                      \
    auto& BOOST_PP_CAT(m_, CLASS) =                                            \
        method<CLASS, auto(virtual_<CLASS&>)->void, test_registry>::fn;

#define ADD_METHOD_N(CLASS, N)                                                 \
    auto& BOOST_PP_CAT(BOOST_PP_CAT(m_, CLASS), N) =                           \
        method<M<N>, auto(virtual_<CLASS&>)->void, test_registry>::fn;

BOOST_AUTO_TEST_CASE(test_assign_slots_a_b1_c) {
    using test_registry = test_registry_<__COUNTER__>;

    /*
      A
     / \
    B1  C
    */

    struct A {
        virtual ~A() = default;
    };
    struct B : A {};
    struct C : A {};

    BOOST_OPENMETHOD_REGISTER(use_classes<A, test_registry>);
    BOOST_OPENMETHOD_REGISTER(use_classes<A, B, test_registry>);
    BOOST_OPENMETHOD_REGISTER(use_classes<A, C, test_registry>);
    ADD_METHOD(B);

    auto comp = initialize<test_registry>();

    BOOST_TEST_REQUIRE(check(comp[m_B])->slots.size() == 1u);
    BOOST_TEST(check(comp[m_B])->slots[0] == 0u);
    BOOST_TEST(get_class<A>(comp)->vtbl.size() == 0u);
    BOOST_TEST(get_class<B>(comp)->vtbl.size() == 1u);
    BOOST_TEST(get_class<C>(comp)->vtbl.size() == 0u);
}

BOOST_AUTO_TEST_CASE(test_assign_slots_a1_b1_c1) {
    using test_registry = test_registry_<__COUNTER__>;

    /*
      A1
     / \
    B1  C1
    */

    struct A {
        virtual ~A() = default;
    };
    struct B : A {};
    struct C : A {};

    BOOST_OPENMETHOD_REGISTER(use_classes<A, test_registry>);
    BOOST_OPENMETHOD_REGISTER(use_classes<A, B, test_registry>);
    BOOST_OPENMETHOD_REGISTER(use_classes<A, C, test_registry>);
    ADD_METHOD(A);
    ADD_METHOD(B);
    ADD_METHOD(C);
    auto comp = initialize<test_registry>();

    BOOST_TEST_REQUIRE(check(comp[m_A])->slots.size() == 1u);
    BOOST_TEST(check(comp[m_A])->slots[0] == 0u);
    BOOST_TEST(get_class<A>(comp)->vtbl.size() == 1u);

    BOOST_TEST_REQUIRE(check(comp[m_B])->slots.size() == 1u);
    BOOST_TEST(check(comp[m_B])->slots[0] == 1u);
    BOOST_TEST(get_class<B>(comp)->vtbl.size() == 2u);

    BOOST_TEST_REQUIRE(check(comp[m_C])->slots.size() == 1u);
    BOOST_TEST(check(comp[m_C])->slots[0] == 1u);
    BOOST_TEST(get_class<C>(comp)->vtbl.size() == 2u);
}

BOOST_AUTO_TEST_CASE(test_assign_slots_a1_b1_d1_c1_d1) {
    using test_registry = test_registry_<__COUNTER__>;

    /*
      A1
     / \
    B1  C1 - slots 0-2 are wasted
     \ /
      D1
    */

    struct A {
        virtual ~A() = default;
    };
    struct B : virtual A {};
    struct C : virtual A {};
    struct D : B, C {};

    BOOST_OPENMETHOD_REGISTER(use_classes<A, test_registry>);
    BOOST_OPENMETHOD_REGISTER(use_classes<A, B, test_registry>);
    BOOST_OPENMETHOD_REGISTER(use_classes<A, C, test_registry>);
    BOOST_OPENMETHOD_REGISTER(use_classes<D, B, C, test_registry>);
    ADD_METHOD(A);
    ADD_METHOD(B);
    ADD_METHOD(C);
    ADD_METHOD(D);
    auto comp = initialize<test_registry>();

    BOOST_TEST_REQUIRE(check(comp[m_A])->slots.size() == 1u);
    BOOST_TEST(check(comp[m_A])->slots[0] == 0u);
    BOOST_TEST(get_class<A>(comp)->vtbl.size() == 1u);

    BOOST_TEST_REQUIRE(check(comp[m_B])->slots.size() == 1u);
    BOOST_TEST(check(comp[m_B])->slots[0] == 1u);
    BOOST_TEST(get_class<B>(comp)->vtbl.size() == 2u);

    BOOST_TEST_REQUIRE(check(comp[m_D])->slots.size() == 1u);
    BOOST_TEST(check(comp[m_D])->slots[0] == 2u);
    BOOST_TEST(get_class<D>(comp)->vtbl.size() == 4u);

    BOOST_TEST_REQUIRE(check(comp[m_C])->slots.size() == 1u);
    BOOST_TEST(check(comp[m_C])->slots[0] == 3u);
    BOOST_TEST(get_class<C>(comp)->vtbl.size() == 4u);
    // slots 0-2 in C are wasted, to make room for methods in B and D
}

BOOST_AUTO_TEST_CASE(test_assign_slots_a1_b1_d1_c1_d1_e2) {
    using test_registry = test_registry_<__COUNTER__>;

    /*
      A1
     / \
    B1  C1 slots 0-2 are wasted
     \ /  \
      D1  E2 but E can use them
    */

    struct A {
        virtual ~A() = default;
    };
    struct B : virtual A {};
    struct C : virtual A {};
    struct E : C {};
    struct D : B, C {};

    BOOST_OPENMETHOD_REGISTER(use_classes<A, test_registry>);
    BOOST_OPENMETHOD_REGISTER(use_classes<A, B, test_registry>);
    BOOST_OPENMETHOD_REGISTER(use_classes<A, C, test_registry>);
    BOOST_OPENMETHOD_REGISTER(use_classes<C, E, test_registry>);
    BOOST_OPENMETHOD_REGISTER(use_classes<D, B, C, test_registry>);
    ADD_METHOD(A);
    ADD_METHOD(B);
    ADD_METHOD(C);
    ADD_METHOD(D);
    ADD_METHOD_N(E, 1);
    ADD_METHOD_N(E, 2);
    ADD_METHOD_N(E, 3);
    auto comp = initialize<test_registry>();

    BOOST_TEST_REQUIRE(check(comp[m_A])->slots.size() == 1u);
    BOOST_TEST(check(comp[m_A])->slots[0] == 0u);
    BOOST_TEST(get_class<A>(comp)->vtbl.size() == 1u);

    BOOST_TEST_REQUIRE(check(comp[m_B])->slots.size() == 1u);
    BOOST_TEST(check(comp[m_B])->slots[0] == 1u);
    BOOST_TEST(get_class<B>(comp)->vtbl.size() == 2u);

    BOOST_TEST_REQUIRE(check(comp[m_D])->slots.size() == 1u);
    BOOST_TEST(check(comp[m_D])->slots[0] == 2u);
    BOOST_TEST(get_class<D>(comp)->vtbl.size() == 4u);

    BOOST_TEST_REQUIRE(check(comp[m_C])->slots.size() == 1u);
    BOOST_TEST(check(comp[m_C])->slots[0] == 3u);
    BOOST_TEST(get_class<C>(comp)->vtbl.size() == 4u);
    // slots 0-2 in C are wasted, to make room for methods in B and D

    BOOST_TEST_REQUIRE(check(comp[m_E1])->slots.size() == 1u);
    BOOST_TEST(check(comp[m_E1])->slots[0] == 1u);

    BOOST_TEST_REQUIRE(check(comp[m_E2])->slots.size() == 1u);
    BOOST_TEST(check(comp[m_E2])->slots[0] == 2u);

    BOOST_TEST_REQUIRE(check(comp[m_E3])->slots.size() == 1u);
    BOOST_TEST(check(comp[m_E3])->slots[0] == 4u);

    BOOST_TEST(get_class<E>(comp)->vtbl.size() == 5u);
}

BOOST_AUTO_TEST_CASE(test_assign_slots_a1_c1_b1) {
    using test_registry = test_registry_<__COUNTER__>;

    /*
    A1  B1
     \  /
      C1
    */

    struct A {
        virtual ~A() = default;
    };
    struct B {
        virtual ~B() = default;
    };
    struct C : A, B {};

    BOOST_OPENMETHOD_REGISTER(use_classes<A, test_registry>);
    BOOST_OPENMETHOD_REGISTER(use_classes<B, test_registry>);
    BOOST_OPENMETHOD_REGISTER(use_classes<A, B, C, test_registry>);
    ADD_METHOD(A);
    ADD_METHOD(B);
    ADD_METHOD(C);
    auto comp = initialize<test_registry>();

    BOOST_TEST_REQUIRE(check(comp[m_A])->slots.size() == 1u);
    BOOST_TEST(check(comp[m_A])->slots[0] == 0u);
    BOOST_TEST(get_class<A>(comp)->vtbl.size() == 1u);

    BOOST_TEST_REQUIRE(check(comp[m_C])->slots.size() == 1u);
    BOOST_TEST(check(comp[m_C])->slots[0] == 1u);
    BOOST_TEST(get_class<C>(comp)->vtbl.size() == 3u);

    BOOST_TEST_REQUIRE(check(comp[m_B])->slots.size() == 1u);
    BOOST_TEST(check(comp[m_B])->slots[0] == 2u);
    BOOST_TEST(get_class<B>(comp)->first_slot == 2u);
    BOOST_TEST(get_class<B>(comp)->vtbl.size() == 1u);
}
