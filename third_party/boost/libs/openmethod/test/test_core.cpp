// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <type_traits>

#define BOOST_TEST_MODULE core
#include <boost/test/unit_test.hpp>

#include <boost/openmethod/preamble.hpp>

using namespace boost::openmethod;
using namespace boost::openmethod::detail;
namespace mp11 = boost::mp11;

#include <boost/openmethod.hpp>
#include <boost/openmethod/inplace_vptr.hpp>
#include <boost/openmethod/interop/std_shared_ptr.hpp>

#include "test_util.hpp"

namespace test_virtual {

struct base {
    virtual ~base() {
    }
};

struct a : base {};
struct b : base {};
struct c : base {};
struct d : base {};
struct e : base {};
struct f : base {};

static_assert(std::is_same_v<
              virtual_traits<base&, default_registry>::virtual_type, base>);

static_assert(
    std::is_same_v<
        virtual_traits<const base&, default_registry>::virtual_type, base>);

static_assert(std::is_same_v<
              virtual_traits<base&&, default_registry>::virtual_type, base>);

static_assert(std::is_same_v<
              mp11::mp_filter<
                  is_virtual, mp11::mp_list<virtual_<a&>, b, virtual_<c&>>>,
              mp11::mp_list<virtual_<a&>, virtual_<c&>>>);

static_assert(std::is_same_v<remove_virtual_<virtual_<a&>>, a&>);

static_assert(std::is_same_v<virtual_type<a&, default_registry>, a>);

static_assert(
    std::is_same_v<
        virtual_types<mp11::mp_list<
            virtual_<std::shared_ptr<a>>, b, virtual_<std::shared_ptr<c>>>>,
        mp11::mp_list<std::shared_ptr<a>, std::shared_ptr<c>>>);

static_assert(std::is_same_v<
              overrider_virtual_types<
                  mp11::mp_list<virtual_<a&>, b, virtual_<c&>>,
                  mp11::mp_list<d&, e, f&>, default_registry>,
              mp11::mp_list<d, f>>);

static_assert(
    std::is_same_v<virtual_type<std::shared_ptr<a>, default_registry>, a>);

static_assert(
    std::is_same_v<
        virtual_traits<virtual_ptr<a>, default_registry>::virtual_type, a>);

static_assert(std::is_same_v<
              select_overrider_virtual_type_aux<
                  virtual_ptr<base>, virtual_ptr<a>, default_registry>::type,
              a>);

static_assert(
    std::is_same_v<
        overrider_virtual_types<
            mp11::mp_list<virtual_ptr<a>, b, virtual_ptr<c>>,
            mp11::mp_list<virtual_ptr<d>, e, virtual_ptr<f>>, default_registry>,
        mp11::mp_list<d, f>>);

static_assert(
    std::is_same_v<
        overrider_virtual_types<
            mp11::mp_list<
                const virtual_ptr<base>&, b, const virtual_ptr<base>&>,
            mp11::mp_list<const virtual_ptr<d>&, e, const virtual_ptr<f>&>,
            default_registry>,
        mp11::mp_list<d, f>>);

static_assert(
    std::is_same_v<
        overrider_virtual_types<
            mp11::mp_list<
                virtual_<std::shared_ptr<a>>, b, virtual_<std::shared_ptr<c>>>,
            mp11::mp_list<std::shared_ptr<d>, e, std::shared_ptr<f>>,
            default_registry>,
        mp11::mp_list<d, f>>);

static_assert(std::is_same_v<
              mp11::mp_transform<
                  remove_virtual_, mp11::mp_list<virtual_<a&>, virtual_<c&>>>,
              mp11::mp_list<a&, c&>>);

static_assert(
    std::is_same_v<
        mp11::mp_transform_q<
            mp11::mp_bind_back<virtual_type, default_registry>,
            mp11::mp_transform<
                remove_virtual_, mp11::mp_list<virtual_<a&>, virtual_<c&>>>>,
        mp11::mp_list<a, c>>);

static_assert(
    std::is_same_v<
        mp11::mp_transform_q<
            mp11::mp_bind_back<virtual_type, default_registry>,
            mp11::mp_transform<
                remove_virtual_,
                mp11::mp_filter<
                    is_virtual, mp11::mp_list<virtual_<a&>, b, virtual_<c&>>>>>,
        mp11::mp_list<a, c>>);

struct registry1 : default_registry::with<unique<registry1>> {};
struct registry2 : default_registry::with<unique<registry2>> {};

struct non_polymorphic_inplace_vptr {};

auto boost_openmethod_vptr(const non_polymorphic_inplace_vptr&, void*)
    -> vptr_type;

// clang-format on

static_assert(std::is_same_v<
              virtual_types<mp11::mp_list<virtual_<a&>, b, virtual_<c&>>>,
              mp11::mp_list<a&, c&>>);

static_assert(detail::is_registry<default_registry>);

struct not_a_policy {};
static_assert(!detail::is_registry<not_a_policy>);

BOOST_AUTO_TEST_CASE(test_init_type_ids) {
    type_id ids[2];
    auto last = init_type_ids<default_registry, mp11::mp_list<a&, b&>>::fn(ids);
    BOOST_TEST_REQUIRE(last - ids == 2);
    BOOST_TEST_REQUIRE(ids[0] == type_id(&typeid(a)));
    BOOST_TEST_REQUIRE(ids[1] == type_id(&typeid(b)));
}

} // namespace test_virtual

namespace test_macros {

// Check that macros can handle commas in parameter and return types.

struct Animal {
    virtual ~Animal() = default;
};

BOOST_OPENMETHOD(poke, (virtual_<Animal&>), std::tuple<int, int>);

} // namespace test_macros

namespace casts {

struct Animal {
    virtual ~Animal() {
    }
    int a{1};
};

struct Mammal : virtual Animal {
    int m{2};
};

struct Carnivore : virtual Animal {
    int c{3};
};

struct Dog : Mammal, Carnivore {
    int d{4};
};

auto mammal_this(const Mammal& obj) -> const void* {
    return &obj;
}

auto carnivore_this(const Carnivore& obj) -> const void* {
    return &obj;
}

auto dog_this(const Dog& obj) -> const void* {
    return &obj;
}

BOOST_AUTO_TEST_CASE(casts) {
    Dog dog;
    const Animal& animal = dog;
    const Mammal& mammal = dog;
    const Carnivore& carnivore = dog;

    BOOST_TEST(
        (&virtual_traits<const Animal&, default_registry>::cast<const Mammal&>(
              animal)
              .m) == &dog.m);
    BOOST_TEST(
        (&virtual_traits<const Animal&, default_registry>::cast<
              const Carnivore&>(animal)
              .c) == &dog.c);
    BOOST_TEST(
        (&virtual_traits<const Animal&, default_registry>::cast<const Mammal&>(
              animal)
              .m) == &dog.m);
    BOOST_TEST(
        (&virtual_traits<const Animal&, default_registry>::cast<const Dog&>(
              animal)
              .d) == &dog.d);
    BOOST_TEST(
        (&virtual_traits<const Mammal&, default_registry>::cast<const Dog&>(
              mammal)
              .d) == &dog.d);
    BOOST_TEST(
        (&virtual_traits<const Carnivore&, default_registry>::cast<const Dog&>(
              carnivore)
              .c) == &dog.c);

    using virtual_animal_t = virtual_type<const Animal&, default_registry>;
    static_assert(std::is_same_v<virtual_animal_t, Animal>, "animal");
    using virtual_mammal_t = virtual_type<const Mammal&, default_registry>;
    static_assert(std::is_same_v<virtual_mammal_t, Mammal>, "mammal");
}

} // namespace casts

namespace test_use_classes {

struct Animal {};
struct Dog : public Animal {};
struct Bulldog : public Dog {};
struct Cat : public Animal {};
struct Dolphin : public Animal {};

static_assert(
    std::is_same_v<
        inheritance_map<Animal, Dog, Bulldog, Cat, Dolphin>,
        mp11::mp_list<
            mp11::mp_list<Animal, Animal>, mp11::mp_list<Dog, Animal, Dog>,
            mp11::mp_list<Bulldog, Animal, Dog, Bulldog>,
            mp11::mp_list<Cat, Animal, Cat>,
            mp11::mp_list<Dolphin, Animal, Dolphin>>>);

static_assert(
    std::is_same_v<
        detail::use_classes_tuple_type<Animal, Dog, Bulldog, Cat, Dolphin>,
        std::tuple<
            use_class_aux<default_registry, mp11::mp_list<Animal, Animal>>,
            use_class_aux<default_registry, mp11::mp_list<Dog, Animal, Dog>>,
            use_class_aux<
                default_registry, mp11::mp_list<Bulldog, Animal, Dog, Bulldog>>,
            use_class_aux<default_registry, mp11::mp_list<Cat, Animal, Cat>>,
            use_class_aux<
                default_registry, mp11::mp_list<Dolphin, Animal, Dolphin>>>>);

} // namespace test_use_classes

namespace TEST_NS {

struct Animal {
    friend auto boost_openmethod_vptr(const Animal&, void*) -> vptr_type;
};

static_assert(detail::has_vptr_fn<Animal, default_registry>);

} // namespace TEST_NS

namespace TEST_NS {

using test_registry = test_registry_<__COUNTER__>;

const detail::word value;

struct Animal {
    friend auto boost_openmethod_vptr(const Animal&, test_registry*) {
        return &value;
    }
};

static_assert(detail::has_vptr_fn<Animal, test_registry>);
static_assert(!detail::has_vptr_fn<Animal, default_registry>);

BOOST_AUTO_TEST_CASE(vptr_from_function) {
    initialize<test_registry>();
    BOOST_TEST(detail::acquire_vptr<test_registry>(Animal{}) == &value);
}

} // namespace TEST_NS
