// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or q at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/openmethod/interop/std_shared_ptr.hpp>
#include <boost/openmethod/interop/boost_intrusive_ptr.hpp>

#define BOOST_TEST_MODULE openmethod
#include <boost/test/unit_test.hpp>

#include <boost/utility/identity_type.hpp>

#include "test_virtual_ptr_value_semantics.hpp"

#include <memory>

#ifdef BOOST_GCC
#pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#endif

#ifdef __clang__
#pragma clang diagnostic ignored "-Wunused-local-typedefs"
#endif

struct using_shared_ptr {
    template<class Class>
    using ptr = std::shared_ptr<Class>;

    template<class Class>
    using virtual_ptr = shared_virtual_ptr<Class>;

    template<class Class, class... Args>
    static decltype(auto) make(Args&&... args) {
        return std::make_shared<Class>(std::forward<Args>(args)...);
    }

    template<class Class, class... Args>
    static decltype(auto) make_virtual(Args&&... args) {
        return make_shared_virtual<Class>(std::forward<Args>(args)...);
    }

    static bool cast_moves() {
        initialize();

        ptr<Animal> animal = make<Dog>();
        (void)std::static_pointer_cast<Dog>(animal);

        return animal.get() == nullptr;
    }
};

struct using_boost_intrusive_ptr {
    template<class Class>
    using ptr = boost::intrusive_ptr<Class>;

    template<class Class>
    using virtual_ptr = boost_intrusive_virtual_ptr<Class>;

    template<class Class, class... Args>
    static auto make(Args&&... args) {
        return boost::intrusive_ptr<Class>(
            new Class(std::forward<Args>(args)...));
    }

    template<class Class, class... Args>
    static auto make_virtual(Args&&... args) {
        return make_boost_intrusive_virtual<Class>(std::forward<Args>(args)...);
    }

    static bool cast_moves() {
        return false;
    }
};

using smart_pointers =
    boost::mp11::mp_list<using_shared_ptr, using_boost_intrusive_ptr>;

#define USING_DECLARATIONS                                                     \
    using animal_ptr = typename smart::template ptr<Animal>;                   \
    using const_animal_ptr = typename smart::template ptr<const Animal>;       \
    using animal_virtual_ptr = typename smart::template virtual_ptr<Animal>;   \
    using const_animal_virtual_ptr =                                           \
        typename smart::template virtual_ptr<const Animal>;                    \
    using dog_ptr = typename smart::template ptr<Dog>;                         \
    using const_dog_ptr = typename smart::template ptr<const Dog>;             \
    using dog_virtual_ptr = typename smart::template virtual_ptr<Dog>;         \
    using const_dog_virtual_ptr =                                              \
        typename smart::template virtual_ptr<const Dog>;                       \
    using cat_ptr = typename smart::template ptr<Cat>;                         \
    using const_cat_ptr = typename smart::template ptr<const Cat>;             \
    using cat_virtual_ptr = typename smart::template virtual_ptr<Cat>;         \
    using const_cat_virtual_ptr =                                              \
        typename smart::template virtual_ptr<const Cat>;

BOOST_AUTO_TEST_CASE_TEMPLATE(
    smart_pointer_value_semantics, smart, smart_pointers) {
    USING_DECLARATIONS;

    static_assert(SameSmartPtr<animal_ptr, dog_ptr, default_registry>);
    static_assert(
        !SameSmartPtr<animal_ptr, std::unique_ptr<Dog>, default_registry>);
    static_assert(
        std::is_same_v<typename animal_virtual_ptr::element_type, Animal>);
    static_assert(std::is_same_v<
                  decltype(std::declval<animal_virtual_ptr>().get()), Animal*>);
    static_assert(IsSmartPtr<animal_ptr, default_registry>);
    static_assert(IsSmartPtr<const_animal_ptr, default_registry>);
    static_assert(
        std::is_same_v<decltype(*std::declval<animal_virtual_ptr>()), Animal&>);

    initialize();

    // construction and assignment from a plain pointer or reference is not
    // allowed

    static_assert(!construct_assign_ok<dog_virtual_ptr, Dog>);
    static_assert(!construct_assign_ok<dog_virtual_ptr, Dog&&>);
    static_assert(!construct_assign_ok<dog_virtual_ptr, const Dog&>);
    static_assert(!construct_assign_ok<dog_virtual_ptr, const Dog*>);

    // -------------------------------------------------------------------------
    // construction and assignment from plain references and pointers

    {
        dog_virtual_ptr p{nullptr};
        BOOST_TEST(p.get() == nullptr);
        BOOST_TEST(p.vptr() == nullptr);
    }

    {
        auto snoopy = smart::template make<Dog>();
        dog_virtual_ptr p(snoopy);
        BOOST_TEST(p.get() == snoopy.get());
        BOOST_TEST(p.vptr() == default_registry::template static_vptr<Dog>);

        p = *&p;
        BOOST_TEST(p.get() == snoopy.get());
        BOOST_TEST(p.vptr() == default_registry::template static_vptr<Dog>);

        auto hector = smart::template make<Dog>();
        p = hector;
        BOOST_TEST(p.get() == hector.get());
        BOOST_TEST(p.vptr() == default_registry::template static_vptr<Dog>);
    }

    {
        auto snoopy = smart::template make<Dog>();
        animal_virtual_ptr p(snoopy);
        BOOST_TEST(p.get() == snoopy.get());
        BOOST_TEST(p.vptr() == default_registry::template static_vptr<Dog>);

        auto felix = smart::template make<Cat>();
        p = felix;
        BOOST_TEST(p.get() == felix.get());
        BOOST_TEST(p.vptr() == default_registry::template static_vptr<Cat>);
    }

    {
        auto snoopy = smart::template make<const Dog>();
        const_dog_virtual_ptr p(snoopy);
        BOOST_TEST(p.get() == snoopy.get());
        BOOST_TEST(p.vptr() == default_registry::template static_vptr<Dog>);

        auto hector = smart::template make<const Dog>();
        p = hector;
        BOOST_TEST(p.get() == hector.get());
        BOOST_TEST(p.vptr() == default_registry::template static_vptr<Dog>);
    }

    {
        const auto snoopy = smart::template make<const Dog>();
        const_animal_virtual_ptr p(snoopy);
        BOOST_TEST(p.get() == snoopy.get());
        BOOST_TEST(p.vptr() == default_registry::template static_vptr<Dog>);

        auto felix = smart::template make<const Cat>();
        p = felix;
        BOOST_TEST(p.get() == felix.get());
        BOOST_TEST(p.vptr() == default_registry::template static_vptr<Cat>);
    }

    {
        auto snoopy = smart::template make<Dog>();
        dog_virtual_ptr p(snoopy);
        BOOST_TEST(p.get() == snoopy.get());
        BOOST_TEST(p.vptr() == default_registry::template static_vptr<Dog>);

        auto hector = smart::template make<Dog>();
        p = hector;
        BOOST_TEST(p.get() == hector.get());
        BOOST_TEST(p.vptr() == default_registry::template static_vptr<Dog>);
    }

    {
        auto snoopy = smart::template make<Dog>();
        animal_virtual_ptr p(snoopy);
        BOOST_TEST(p.get() == snoopy.get());
        BOOST_TEST(p.vptr() == default_registry::template static_vptr<Dog>);

        auto felix = smart::template make<Cat>();
        p = felix;
        BOOST_TEST(p.get() == felix.get());
        BOOST_TEST(p.vptr() == default_registry::template static_vptr<Cat>);
    }

    {
        auto snoopy = smart::template make<const Dog>();
        const_dog_virtual_ptr p(snoopy);
        BOOST_TEST(p.get() == snoopy.get());
        BOOST_TEST(p.vptr() == default_registry::template static_vptr<Dog>);

        auto hector = smart::template make<const Dog>();
        p = hector;
        BOOST_TEST(p.get() == hector.get());
        BOOST_TEST(p.vptr() == default_registry::template static_vptr<Dog>);
    }

    {
        auto snoopy = smart::template make<const Dog>();
        const_animal_virtual_ptr p(snoopy);
        BOOST_TEST(p.get() == snoopy.get());
        BOOST_TEST(p.vptr() == default_registry::template static_vptr<Dog>);

        auto felix = smart::template make<const Cat>();
        p = felix;
        BOOST_TEST(p.get() == felix.get());
        BOOST_TEST(p.vptr() == default_registry::template static_vptr<Cat>);
    }

    // dog_virtual_ptr p{Dog()};
    static_assert(!construct_assign_ok<dog_virtual_ptr, Dog&&>);

    // -------------------------------------------------------------------------
    // construction and assignment from other shared_virtual_ptr

    {
        // dog_virtual_ptr(const dog_virtual_ptr&)
        auto snoopy = smart::template make<Dog>();
        const dog_virtual_ptr p(snoopy);
        dog_virtual_ptr q(p);
        BOOST_TEST(q.get() == snoopy.get());
        BOOST_TEST(q.vptr() == default_registry::template static_vptr<Dog>);
    }

    {
        // dog_virtual_ptr(dog_virtual_ptr&)
        auto snoopy = smart::template make<Dog>();
        dog_virtual_ptr p(snoopy);
        dog_virtual_ptr q(p);
        BOOST_TEST(q.get() == snoopy.get());
        BOOST_TEST(q.vptr() == default_registry::template static_vptr<Dog>);
    }

    {
        // dog_virtual_ptr(dog_virtual_ptr&&)
        auto snoopy = smart::template make<Dog>();
        dog_virtual_ptr p(snoopy);
        dog_virtual_ptr q(std::move(p));
        BOOST_TEST(q.get() == snoopy.get());
        BOOST_TEST(q.vptr() == default_registry::template static_vptr<Dog>);
        // coverity[use_after_move]
        BOOST_TEST(p.get() == nullptr);
        BOOST_TEST(p.vptr() == nullptr);
    }

    {
        // animal_virtual_ptr(const dog_virtual_ptr&)
        auto snoopy = smart::template make<Dog>();
        const dog_virtual_ptr p(snoopy);
        animal_virtual_ptr base(p);
        BOOST_TEST(base.get() == snoopy.get());
        BOOST_TEST(base.vptr() == default_registry::template static_vptr<Dog>);
    }

    {
        // shared_virtual_ptr<const Dog>(const dog_virtual_ptr&)
        auto snoopy = smart::template make<Dog>();
        const dog_virtual_ptr p(snoopy);
        const_dog_virtual_ptr const_q(p);
        BOOST_TEST(const_q.get() == snoopy.get());
        BOOST_TEST(
            const_q.vptr() == default_registry::template static_vptr<Dog>);
    }

    {
        // shared_virtual_ptr<const Animal>(const dog_virtual_ptr&)
        auto snoopy = smart::template make<Dog>();
        const dog_virtual_ptr p(snoopy);
        const_animal_virtual_ptr const_base_q(p);
        BOOST_TEST(const_base_q.get() == snoopy.get());
        BOOST_TEST(
            const_base_q.vptr() == default_registry::template static_vptr<Dog>);
    }

    {
        // dog_virtual_ptr()
        dog_virtual_ptr p{nullptr};
        BOOST_TEST(p.get() == nullptr);
        BOOST_TEST(p.vptr() == nullptr);
    }

    {
        dog_virtual_ptr p{dog_ptr()};
        BOOST_TEST(p.get() == nullptr);
        BOOST_TEST(p.vptr() == nullptr);
    }

    // -------------------------------------------------------------------------
    // assignment

    {
        dog_virtual_ptr p;
        auto snoopy = smart::template make<Dog>();
        p = snoopy;
        BOOST_TEST(p.get() == snoopy.get());
        BOOST_TEST(p.vptr() == default_registry::template static_vptr<Dog>);
    }

    {
        dog_virtual_ptr p;
        auto snoopy = smart::template make<Dog>();
        p = snoopy;
        BOOST_TEST(p.get() == snoopy.get());
        BOOST_TEST(p.vptr() == default_registry::template static_vptr<Dog>);
    }

    {
        auto p = smart::template make_virtual<Dog>();
        p = nullptr;
        BOOST_TEST(p.get() == nullptr);
        BOOST_TEST(p.vptr() == nullptr);
    }

    {
        auto p = smart::template make_virtual<Dog>();
        p = dog_ptr();
        BOOST_TEST(p.get() == nullptr);
        BOOST_TEST(p.vptr() == nullptr);
    }

    static_assert(!construct_assign_ok<dog_virtual_ptr, const Dog&>);
    static_assert(!construct_assign_ok<dog_virtual_ptr, const Dog*>);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(cast_smart_pointer_value, smart, smart_pointers) {
    USING_DECLARATIONS;

    initialize();

    animal_ptr animal = smart::template make<Dog>();
    dog_ptr dog =
        virtual_traits<animal_ptr, default_registry>::template cast<dog_ptr>(
            animal);
    BOOST_TEST(dog.get() == animal.get());
}

BOOST_AUTO_TEST_CASE_TEMPLATE(
    cast_smart_ptr_lvalue_reference, smart, smart_pointers) {
    USING_DECLARATIONS;

    initialize();

    animal_ptr animal = smart::template make<Dog>();
    dog_ptr dog =
        virtual_traits<const animal_ptr&, default_registry>::template cast<
            const dog_ptr&>(animal);
    BOOST_TEST(dog.get() == animal.get());

    dog_ptr dog2 = virtual_traits<
        const dog_ptr&, default_registry>::template cast<const dog_ptr&>(dog);
    BOOST_TEST(dog2.get() == dog.get());
}

BOOST_AUTO_TEST_CASE_TEMPLATE(
    cast_smart_ptr_xvalue_reference, smart, smart_pointers) {
    USING_DECLARATIONS;

    initialize();

    animal_ptr animal = smart::template make<Dog>();
    auto p = animal.get();
    auto dog =
        virtual_traits<animal_ptr, default_registry>::template cast<dog_ptr>(
            std::move(animal));
    BOOST_TEST(dog.get() == p);

    if (smart::cast_moves()) {
        // coverity[use_after_move]
        BOOST_TEST(animal.get() == nullptr);
    }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(
    cast_shared_virtual_ptr_value, smart, smart_pointers) {
    USING_DECLARATIONS;

    initialize();

    animal_virtual_ptr base = smart::template make<Dog>();
    auto derived = virtual_traits<
        animal_virtual_ptr, default_registry>::template cast<dog_ptr>(base);
    BOOST_TEST(derived.get() == base.get());
    BOOST_TEST(base.vptr() == default_registry::static_vptr<Dog>);
    BOOST_TEST(derived.vptr() == default_registry::static_vptr<Dog>);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(
    cast_shared_virtual_ptr_lvalue_reference, smart, smart_pointers) {
    USING_DECLARATIONS;

    initialize();

    animal_virtual_ptr base = smart::template make<Dog>();
    auto derived = virtual_traits<const animal_virtual_ptr&, default_registry>::
        template cast<const dog_virtual_ptr&>(base);
    BOOST_TEST(derived.get() == base.get());
    BOOST_TEST(base.vptr() == default_registry::static_vptr<Dog>);
    BOOST_TEST(derived.vptr() == default_registry::static_vptr<Dog>);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(
    cast_shared_virtual_ptr_xvalue_reference, smart, smart_pointers) {
    USING_DECLARATIONS;

    initialize();

    animal_virtual_ptr base = smart::template make<Dog>();
    auto p = base.get();
    auto derived =
        virtual_traits<animal_virtual_ptr, default_registry>::template cast<
            dog_virtual_ptr>(std::move(base));
    BOOST_TEST(derived.get() == p);
    BOOST_TEST(derived.vptr() == default_registry::static_vptr<Dog>);

    if (smart::cast_moves()) {
        BOOST_TEST(base.get() == nullptr);
    }
}

template struct check_illegal_smart_ops<
    std::shared_ptr, std::unique_ptr, direct_vector>;

template struct check_illegal_smart_ops<
    boost::intrusive_ptr, std::unique_ptr, direct_vector>;
