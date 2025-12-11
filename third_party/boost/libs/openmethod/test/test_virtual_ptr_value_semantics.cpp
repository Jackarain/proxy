// qright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or q at http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_TEST_MODULE openmethod
#include <boost/test/unit_test.hpp>

#include "test_virtual_ptr_value_semantics.hpp"

BOOST_AUTO_TEST_CASE_TEMPLATE(
    plain_virtual_ptr_value, Registry, test_policies) {
    static_assert(
        std::is_same_v<
            typename virtual_ptr<Animal, Registry>::element_type, Animal>);
    static_assert(std::is_same_v<
                  typename virtual_ptr<const Animal, Registry>::element_type,
                  const Animal>);
    static_assert(std::is_same_v<
                  decltype(std::declval<virtual_ptr<Animal, Registry>>().get()),
                  Animal*>);
    static_assert(!IsSmartPtr<Animal, Registry>);
    static_assert(!IsSmartPtr<const Animal, Registry>);
    static_assert(
        std::is_same_v<
            decltype(*std::declval<virtual_ptr<Animal, Registry>>()), Animal&>);

    init_test<Registry>();

    // -------------------------------------------------------------------------
    // construction and assignment from plain references and pointers

    {
        virtual_ptr<Dog, Registry> p{nullptr};
        BOOST_TEST(p.get() == nullptr);
        BOOST_TEST(p.vptr() == nullptr);
    }

    {
        Dog snoopy;
        virtual_ptr<Dog, Registry> p(snoopy);
        BOOST_TEST(p.get() == &snoopy);
        BOOST_TEST(p.vptr() == Registry::template static_vptr<Dog>);

        Dog hector;
        p = hector;
        BOOST_TEST(p.get() == &hector);
        BOOST_TEST(p.vptr() == Registry::template static_vptr<Dog>);
    }

    {
        Dog snoopy;
        virtual_ptr<Animal, Registry> p(snoopy);
        BOOST_TEST(p.get() == &snoopy);
        BOOST_TEST(p.vptr() == Registry::template static_vptr<Dog>);

        Cat felix;
        p = felix;
        BOOST_TEST(p.get() == &felix);
        BOOST_TEST(p.vptr() == Registry::template static_vptr<Cat>);
    }

    {
        const Dog snoopy;
        virtual_ptr<const Dog, Registry> p(snoopy);
        BOOST_TEST(p.get() == &snoopy);
        BOOST_TEST(p.vptr() == Registry::template static_vptr<Dog>);

        const Dog hector;
        p = hector;
        BOOST_TEST(p.get() == &hector);
        BOOST_TEST(p.vptr() == Registry::template static_vptr<Dog>);
    }

    {
        const Dog snoopy;
        virtual_ptr<const Animal, Registry> p(snoopy);
        BOOST_TEST(p.get() == &snoopy);
        BOOST_TEST(p.vptr() == Registry::template static_vptr<Dog>);

        const Cat felix;
        p = felix;
        BOOST_TEST(p.get() == &felix);
        BOOST_TEST(p.vptr() == Registry::template static_vptr<Cat>);
    }

    {
        Dog snoopy;
        virtual_ptr<Dog, Registry> p(&snoopy);
        BOOST_TEST(p.get() == &snoopy);
        BOOST_TEST(p.vptr() == Registry::template static_vptr<Dog>);

        Dog hector;
        p = &hector;
        BOOST_TEST(p.get() == &hector);
        BOOST_TEST(p.vptr() == Registry::template static_vptr<Dog>);
    }

    {
        Dog snoopy;
        virtual_ptr<Animal, Registry> p(&snoopy);
        BOOST_TEST(p.get() == &snoopy);
        BOOST_TEST(p.vptr() == Registry::template static_vptr<Dog>);

        Cat felix;
        p = &felix;
        BOOST_TEST(p.get() == &felix);
        BOOST_TEST(p.vptr() == Registry::template static_vptr<Cat>);
    }

    {
        const Dog snoopy;
        virtual_ptr<const Dog, Registry> p(&snoopy);
        BOOST_TEST(p.get() == &snoopy);
        BOOST_TEST(p.vptr() == Registry::template static_vptr<Dog>);

        const Dog hector;
        p = &hector;
        BOOST_TEST(p.get() == &hector);
        BOOST_TEST(p.vptr() == Registry::template static_vptr<Dog>);
    }

    {
        const Dog snoopy;
        virtual_ptr<const Animal, Registry> p(&snoopy);
        BOOST_TEST(p.get() == &snoopy);
        BOOST_TEST(p.vptr() == Registry::template static_vptr<Dog>);

        const Cat felix;
        p = &felix;
        BOOST_TEST(p.get() == &felix);
        BOOST_TEST(p.vptr() == Registry::template static_vptr<Cat>);
    }

    {
        const Dog snoopy;
        auto p = final_virtual_ptr<Registry>(snoopy);
        BOOST_TEST(p.get() == &snoopy);
        BOOST_TEST(p.vptr() == Registry::template static_vptr<Dog>);
    }

    // virtual_ptr<Dog, Registry> p{Dog()};
    static_assert(!construct_assign_ok<virtual_ptr<Dog, Registry>, Dog&&>);

    // -------------------------------------------------------------------------
    // construction and assignment from other virtual_ptr

    {
        // virtual_ptr<Dog>(const virtual_ptr<Dog>&)
        Dog snoopy;
        const virtual_ptr<Dog, Registry> p(snoopy);
        virtual_ptr<Dog, Registry> q(p);
        BOOST_TEST(q.get() == &snoopy);
        BOOST_TEST(q.vptr() == Registry::template static_vptr<Dog>);
    }

    {
        // virtual_ptr<Dog>(virtual_ptr<Dog>&)
        Dog snoopy;
        virtual_ptr<Dog, Registry> p(snoopy);
        virtual_ptr<Dog, Registry> q(p);
        BOOST_TEST(q.get() == &snoopy);
        BOOST_TEST(q.vptr() == Registry::template static_vptr<Dog>);
    }

    {
        // virtual_ptr<Dog>(virtual_ptr<Dog>&&)
        Dog snoopy;
        virtual_ptr<Dog, Registry> p(snoopy);
        virtual_ptr<Dog, Registry> q(std::move(p));
        BOOST_TEST(q.get() == &snoopy);
        BOOST_TEST(q.vptr() == Registry::template static_vptr<Dog>);
        BOOST_TEST(p.get() == &snoopy);
        BOOST_TEST(p.vptr() == Registry::template static_vptr<Dog>);
    }

    {
        // virtual_ptr<Animal>(const virtual_ptr<Dog>&)
        Dog snoopy;
        const virtual_ptr<Dog, Registry> p(snoopy);
        virtual_ptr<Animal, Registry> base(p);
        BOOST_TEST(base.get() == &snoopy);
        BOOST_TEST(base.vptr() == Registry::template static_vptr<Dog>);
    }

    {
        // virtual_ptr<const Dog>(const virtual_ptr<Dog>&)
        Dog snoopy;
        const virtual_ptr<Dog, Registry> p(snoopy);
        virtual_ptr<const Dog, Registry> const_q(p);
        BOOST_TEST(const_q.get() == &snoopy);
        BOOST_TEST(const_q.vptr() == Registry::template static_vptr<Dog>);
    }

    {
        // virtual_ptr<const Animal>(const virtual_ptr<Dog>&)
        Dog snoopy;
        const virtual_ptr<Dog, Registry> p(snoopy);
        virtual_ptr<const Animal, Registry> const_base_q(p);
        BOOST_TEST(const_base_q.get() == &snoopy);
        BOOST_TEST(const_base_q.vptr() == Registry::template static_vptr<Dog>);
    }

    {
        // virtual_ptr<Dog>()
        virtual_ptr<Dog, Registry> p{nullptr};
        BOOST_TEST(p.get() == nullptr);
        BOOST_TEST(p.vptr() == nullptr);
    }

    // -------------------------------------------------------------------------
    // assignment

    {
        virtual_ptr<Dog, Registry> p;
        Dog snoopy;
        p = snoopy;
        BOOST_TEST(p.get() == &snoopy);
        BOOST_TEST(p.vptr() == Registry::template static_vptr<Dog>);
    }

    {
        virtual_ptr<Dog, Registry> p;
        Dog snoopy;
        p = &snoopy;
        BOOST_TEST(p.get() == &snoopy);
        BOOST_TEST(p.vptr() == Registry::template static_vptr<Dog>);
    }

    {
        virtual_ptr<Animal, Registry> p;
        Dog snoopy;
        p = snoopy;
        BOOST_TEST(p.get() == &snoopy);
        BOOST_TEST(p.vptr() == Registry::template static_vptr<Dog>);
    }

    {
        virtual_ptr<Animal, Registry> p;
        Dog snoopy;
        p = &snoopy;
        BOOST_TEST(p.get() == &snoopy);
        BOOST_TEST(p.vptr() == Registry::template static_vptr<Dog>);
    }

    {
        Dog snoopy;
        virtual_ptr<Animal, Registry> p(snoopy);
        p = nullptr;
        BOOST_TEST(p.get() == nullptr);
        BOOST_TEST(p.vptr() == nullptr);
    }

    static_assert(!construct_assign_ok<virtual_ptr<Dog, Registry>, const Dog&>);
    static_assert(!construct_assign_ok<virtual_ptr<Dog, Registry>, const Dog*>);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(indirect_virtual_ptr, Registry, test_policies) {
    BOOST_TEST_MESSAGE(
        "Registry = " << boost::core::demangle(typeid(Registry).name()));

    init_test<Registry>();

    Dog snoopy;
    virtual_ptr<Dog, Registry> p(snoopy);

    BOOST_TEST_MESSAGE("After first call to initialize:");
    BOOST_TEST_MESSAGE("p.vptr() = " << p.vptr());
    BOOST_TEST_MESSAGE(
        "static_vptr<Dog> = " << Registry::template static_vptr<Dog>);
    BOOST_TEST(p.vptr() == Registry::template static_vptr<Dog>);

    // Add a class, to make sure dispatch data is not re-constructed in the same
    // place with the same values:
    struct Cat : Animal {};
    BOOST_OPENMETHOD_CLASSES(Animal, Cat, Registry);

    init_test<Registry>();

    BOOST_TEST_MESSAGE("After second call to initialize:");
    BOOST_TEST_MESSAGE("p.vptr() = " << p.vptr());
    BOOST_TEST_MESSAGE(
        "static_vptr<Dog> = " << Registry::template static_vptr<Dog>);

    if constexpr (Registry::has_indirect_vptr) {
        BOOST_TEST(p.vptr() == Registry::template static_vptr<Dog>);
    } else {
        BOOST_TEST(p.vptr() != Registry::template static_vptr<Dog>);
    }
}

BOOST_AUTO_TEST_CASE(virtual_ptr_final_error) {
    auto prev_handler =
        default_registry::error_handler::set([](const auto& ev) {
            if (auto error = std::get_if<final_error>(&ev)) {
                static_assert(
                    std::is_same_v<decltype(error), const final_error*>);
                throw *error;
            }
        });

    init_test<default_registry>();
    bool threw = false;

    try {
        Dog snoopy;
        Animal& animal = snoopy;
        virtual_ptr<Animal>::final(animal);
    } catch (const final_error& error) {
        default_registry::error_handler::set(prev_handler);
        BOOST_TEST(error.static_type == &typeid(Animal));
        BOOST_TEST(error.dynamic_type == &typeid(Dog));
        threw = true;
    } catch (...) {
        default_registry::error_handler::set(prev_handler);
        BOOST_FAIL("wrong exception");
        return;
    }

    if constexpr (default_registry::has_runtime_checks) {
        if (!threw) {
            BOOST_FAIL("should have thrown");
        }
    } else {
        if (threw) {
            BOOST_FAIL("should not have thrown");
        }
    }
}

// Cannot construct or assign a virtual_ptr from a non-polymorphic object.
static_assert(
    !construct_assign_ok<virtual_ptr<NonPolymorphic>, const NonPolymorphic&>);
static_assert(
    !construct_assign_ok<virtual_ptr<NonPolymorphic>, NonPolymorphic&>);
static_assert(
    !construct_assign_ok<virtual_ptr<NonPolymorphic>, NonPolymorphic&&>);
static_assert(
    !construct_assign_ok<virtual_ptr<NonPolymorphic>, const NonPolymorphic*>);
static_assert(
    !construct_assign_ok<virtual_ptr<NonPolymorphic>, NonPolymorphic*>);
// OK from another virtual_ptr though, because it can be constructed using
// 'final'.
static_assert(construct_assign_ok<
              virtual_ptr<NonPolymorphic>, virtual_ptr<NonPolymorphic>>);
