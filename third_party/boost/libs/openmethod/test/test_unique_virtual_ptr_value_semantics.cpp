// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/openmethod/interop/std_unique_ptr.hpp>

#define BOOST_TEST_MODULE openmethod
#include <boost/test/unit_test.hpp>

#include "test_virtual_ptr_value_semantics.hpp"

#include <memory>

using namespace detail;

BOOST_AUTO_TEST_CASE_TEMPLATE(
    unique_virtual_ptr_value, Registry, test_policies) {
    init_test<Registry>();

    static_assert(std::is_same_v<
                  typename unique_virtual_ptr<Animal, Registry>::element_type,
                  Animal>);
    static_assert(std::is_same_v<
                  decltype(std::declval<unique_virtual_ptr<Animal, Registry>>()
                               .get()),
                  Animal*>);
    static_assert(IsSmartPtr<std::unique_ptr<Animal>, Registry>);
    static_assert(IsSmartPtr<std::unique_ptr<const Animal>, Registry>);
    static_assert(
        std::is_same_v<
            decltype(*std::declval<unique_virtual_ptr<Animal, Registry>>()),
            Animal&>);

    {
        // unique_virtual_ptr<Dog>(nullptr)
        unique_virtual_ptr<Dog, Registry> p{nullptr};
        BOOST_TEST(p.get() == nullptr);
        BOOST_TEST(p.vptr() == nullptr);
    }

    static_assert(!construct_assign_ok<unique_virtual_ptr<Dog, Registry>, Dog>);

    static_assert(
        !construct_assign_ok<unique_virtual_ptr<Dog, Registry>, Dog&>);

    static_assert(
        !construct_assign_ok<unique_virtual_ptr<Dog, Registry>, Dog*>);

    static_assert(!construct_assign_ok<
                  unique_virtual_ptr<Dog, Registry>, std::unique_ptr<Dog>&>);

    static_assert(
        !construct_assign_ok<
            unique_virtual_ptr<Dog, Registry>, const std::unique_ptr<Dog>&>);

    static_assert(!construct_assign_ok<
                  unique_virtual_ptr<Dog, Registry>, unique_virtual_ptr<Dog>>);

    static_assert(!construct_assign_ok<
                  unique_virtual_ptr<Dog, Registry>, unique_virtual_ptr<Dog>&>);

    static_assert(
        !construct_assign_ok<
            unique_virtual_ptr<Dog, Registry>, const unique_virtual_ptr<Dog>&>);

    {
        // construct from unique_ptr temporary
        unique_virtual_ptr<Dog, Registry> p(std::make_unique<Dog>());
        BOOST_TEST(p.vptr() == Registry::template static_vptr<Dog>);
    }

    {
        // derived-to-base ok?
        unique_virtual_ptr<Animal, Registry> p(std::make_unique<Dog>());
        BOOST_TEST(p.vptr() == Registry::template static_vptr<Dog>);
    }

    static_assert(
        !construct_assign_ok<
            unique_virtual_ptr<Dog, Registry>, const std::unique_ptr<Dog>&>);

    static_assert(!construct_assign_ok<
                  unique_virtual_ptr<Dog, Registry>, std::unique_ptr<Dog>&>);

    static_assert(!construct_assign_ok<
                  unique_virtual_ptr<Dog, Registry>,
                  const unique_virtual_ptr<Dog, Registry>&>);

    static_assert(!construct_assign_ok<
                  unique_virtual_ptr<Dog, Registry>,
                  unique_virtual_ptr<Dog, Registry>&>);

    static_assert(!construct_assign_ok<
                  unique_virtual_ptr<Dog, Registry>,
                  unique_virtual_ptr<Dog, Registry>&>);

    {
        // assign from smart ptr temporary
        unique_virtual_ptr<Dog, Registry> p{nullptr};
        p = std::make_unique<Dog>();
        BOOST_TEST(p.get() != nullptr);
        BOOST_TEST(p.vptr() == Registry::template static_vptr<Dog>);
    }

    {
        // derived-to-base ok?
        unique_virtual_ptr<Animal, Registry> p{nullptr};
        p = std::make_unique<Dog>();
        BOOST_TEST(p.get() != nullptr);
        BOOST_TEST(p.vptr() == Registry::template static_vptr<Dog>);
    }

    {
        // unique_virtual_ptr<Dog>(unique_virtual_ptr<Dog>)
        unique_virtual_ptr<Dog, Registry> p(std::make_unique<Dog>());
        auto dog = p.get();
        unique_virtual_ptr<Dog, Registry> q(std::move(p));
        BOOST_TEST(q.get() == dog);
        BOOST_TEST(q.vptr() == Registry::template static_vptr<Dog>);
        BOOST_TEST(p.get() == nullptr);
        BOOST_TEST(p.vptr() == nullptr);
    }

    {
        // derived-to-base ok?
        unique_virtual_ptr<Dog, Registry> p(std::make_unique<Dog>());
        auto dog = p.get();
        unique_virtual_ptr<Animal, Registry> q(std::move(p));
        BOOST_TEST(q.get() == dog);
        BOOST_TEST(q.vptr() == Registry::template static_vptr<Dog>);
        BOOST_TEST(p.get() == nullptr);
        BOOST_TEST(p.vptr() == nullptr);
    }

    {
        // virtual_ptr<Dog>(std::unique_ptr<Dog>())
        unique_virtual_ptr<Dog, Registry> p = std::unique_ptr<Dog>();
        BOOST_TEST(p.get() == nullptr);
        BOOST_TEST(p.vptr() == nullptr);
    }

    {
        unique_virtual_ptr<Dog, Registry> p(std::make_unique<Dog>());
        p = nullptr;
        BOOST_TEST(p.get() == nullptr);
        BOOST_TEST(p.vptr() == nullptr);
    }

    {
        unique_virtual_ptr<Dog, Registry> p(std::make_unique<Dog>());
        p = std::unique_ptr<Dog>();
        BOOST_TEST(p.get() == nullptr);
        BOOST_TEST(p.vptr() == nullptr);
    }

#if 0

    {
        // unique_virtual_ptr<Dog> = const std::unique_ptr<Dog>&
        unique_virtual_ptr<Dog, Registry> p;
        const auto s = std::make_unique<Dog>();
        p = s;
        BOOST_TEST(p.get() == s.get());
        BOOST_TEST(p.vptr() == Registry::template static_vptr<Dog>);
    }

    {
        // unique_virtual_ptr<Dog> = std::unique_ptr<Dog>&
        unique_virtual_ptr<Dog, Registry> p;
        auto s = std::make_unique<Dog>();
        p = s;
        BOOST_TEST(p.get() == s.get());
        BOOST_TEST(p.vptr() == Registry::template static_vptr<Dog>);
    }

    {
        // unique_virtual_ptr<Dog> = std::unique_ptr<Dog>&&
        auto s = std::make_unique<Dog>();
        auto p = s;
        unique_virtual_ptr<Dog, Registry> q;
        q = std::move(p);
        BOOST_TEST(q.get() == s.get());
        BOOST_TEST(q.vptr() == Registry::template static_vptr<Dog>);
        BOOST_TEST(p.get() == nullptr);
    }

    {
        // unique_virtual_ptr<Animal> = std::unique_ptr<Dog>&&
        auto s = std::make_unique<Dog>();
        auto p = s;
        unique_virtual_ptr<Animal, Registry> q;
        q = std::move(p);
        BOOST_TEST(q.get() == s.get());
        BOOST_TEST(q.vptr() == Registry::template static_vptr<Dog>);
        BOOST_TEST(p.get() == nullptr);
    }

    {
        // unique_virtual_ptr<Dog> = unique_virtual_ptr<Dog>&&
        auto s = std::make_unique<Dog>();
        unique_virtual_ptr<Dog, Registry> p(s);
        unique_virtual_ptr<Dog, Registry> q;
        q = std::move(p);
        BOOST_TEST(q.get() == s.get());
        BOOST_TEST(q.vptr() == Registry::template static_vptr<Dog>);
        BOOST_TEST(p.get() == nullptr);
        BOOST_TEST(p.vptr() == nullptr);
    }

    {
        // unique_virtual_ptr<Animal> = unique_virtual_ptr<Dog>&&
        auto s = std::make_unique<Dog>();
        unique_virtual_ptr<Dog, Registry> p(s);
        unique_virtual_ptr<Animal, Registry> q;
        q = std::move(p);
        BOOST_TEST(q.get() == s.get());
        BOOST_TEST(q.vptr() == Registry::template static_vptr<Dog>);
        BOOST_TEST(p.get() == nullptr);
        BOOST_TEST(p.vptr() == nullptr);
    }

    {
        // virtual_ptr<Dog>(unique_virtual_ptr<Dog>&)
        auto p = make_unique_virtual<Dog, Registry>();
        virtual_ptr<Dog, Registry> q(p);
        BOOST_TEST(q.get() == p.get());
        BOOST_TEST(q.vptr() == Registry::template static_vptr<Dog>);
    }

    {
        // virtual_ptr<Dog> = unique_virtual_ptr<Dog>&
        const auto p = make_unique_virtual<Dog, Registry>();
        virtual_ptr<Dog, Registry> q;
        q = p;
        BOOST_TEST(q.get() == p.get());
        BOOST_TEST(q.vptr() == Registry::template static_vptr<Dog>);
    }

    // illegal constructions and assignments
    static_assert(
        !construct_assign_ok<unique_virtual_ptr<Dog, Registry>, Dog>);
    static_assert(
        !construct_assign_ok<unique_virtual_ptr<Dog, Registry>, Dog&&>);
    static_assert(
        !construct_assign_ok<unique_virtual_ptr<Dog, Registry>, const Dog&>);
    static_assert(
        !construct_assign_ok<unique_virtual_ptr<Dog, Registry>, const Dog*>);

    static_assert(!std::is_assignable_v<unique_virtual_ptr<Dog, Registry>, Dog>);
    static_assert(
        !std::is_assignable_v<unique_virtual_ptr<Dog, Registry>, Dog&&>);
    static_assert(
        !std::is_assignable_v<unique_virtual_ptr<Dog, Registry>, const Dog&>);
    static_assert(
        !std::is_assignable_v<unique_virtual_ptr<Dog, Registry>, const Dog*>);

#endif
}

BOOST_AUTO_TEST_CASE_TEMPLATE(
    cast_unique_virtual_ptr_value, Class, test_classes) {
    init_test<default_registry>();

    unique_virtual_ptr<Animal> base = make_unique_virtual<Class>();
    auto p = base.get();
    auto derived =
        virtual_traits<unique_virtual_ptr<Animal>, default_registry>::cast<
            unique_virtual_ptr<Class>>(std::move(base));
    BOOST_TEST(derived.get() == p);
    BOOST_TEST(derived.vptr() == default_registry::static_vptr<Class>);
    BOOST_TEST(base.get() == nullptr);
}

template struct check_illegal_smart_ops<
    std::unique_ptr, std::shared_ptr, direct_vector>;

// Cannot construct or assign a virtual_ptr from a non-polymorphic object.
static_assert(!construct_assign_ok<
              virtual_ptr<std::unique_ptr<NonPolymorphic>>,
              const std::unique_ptr<NonPolymorphic>&>);
static_assert(!construct_assign_ok<
              virtual_ptr<std::unique_ptr<NonPolymorphic>>,
              std::unique_ptr<NonPolymorphic>&>);
static_assert(!construct_assign_ok<
              virtual_ptr<std::unique_ptr<NonPolymorphic>>,
              std::unique_ptr<NonPolymorphic>&&>);
// OK to move from another virtual_ptr though, because it can be constructed
// using 'final'.
static_assert(construct_assign_ok<
              virtual_ptr<std::unique_ptr<NonPolymorphic>>,
              virtual_ptr<std::unique_ptr<NonPolymorphic>>&&>);
