// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/openmethod.hpp>
#include <boost/openmethod/interop/std_shared_ptr.hpp>
#include <boost/openmethod/interop/std_unique_ptr.hpp>
#include <boost/openmethod/initialize.hpp>

#include <iostream>
#include <memory>
#include <string>

#define BOOST_TEST_MODULE openmethod
#include <boost/test/unit_test.hpp>
#include <boost/test/tools/output_test_stream.hpp>

#include "test_util.hpp"

using boost::mp11::mp_list;
using std::cout;
using namespace boost::openmethod;
using namespace boost::openmethod::detail;

namespace using_polymorphic_classes {

struct Animal {
    virtual ~Animal() {
    }
};

struct Dog : Animal {};

BOOST_OPENMETHOD_CLASSES(Animal, Dog);

namespace BOOST_OPENMETHOD_GENSYM {

BOOST_OPENMETHOD(poke, (const virtual_ptr<Animal>&, std::ostream&), void);

BOOST_OPENMETHOD_OVERRIDE(
    poke, (const virtual_ptr<Dog>&, std::ostream& os), void) {
    os << "bark";
}

static_assert(sizeof(virtual_ptr<Animal>) == 2 * sizeof(void*));

BOOST_AUTO_TEST_CASE(test_virtual_ptr_by_ref) {
    boost::openmethod::initialize();

    {
        boost::test_tools::output_test_stream os;
        Dog dog;
        virtual_ptr<Animal> vptr(dog);
        poke(vptr, os);
    }

    {
        // Using  deduction guide.
        boost::test_tools::output_test_stream os;
        Animal&& animal = Dog();
        auto vptr = virtual_ptr(animal);
        poke(vptr, os);
        BOOST_CHECK(os.is_equal("bark"));
    }

    {
        // Using conversion ctor.
        boost::test_tools::output_test_stream os;
        Animal&& animal = Dog();
        poke(animal, os);
        BOOST_CHECK(os.is_equal("bark"));
    }
}

} // namespace BOOST_OPENMETHOD_GENSYM

namespace BOOST_OPENMETHOD_GENSYM {

BOOST_OPENMETHOD(poke, (virtual_ptr<Animal>, std::ostream&), void);

BOOST_OPENMETHOD_OVERRIDE(poke, (virtual_ptr<Dog>, std::ostream& os), void) {
    os << "bark";
}

BOOST_AUTO_TEST_CASE(test_virtual_shared_by_value) {
    boost::openmethod::initialize();

    {
        boost::test_tools::output_test_stream os;
        shared_virtual_ptr<Animal> animal = make_shared_virtual<Dog>();
        poke(animal, os);
        BOOST_CHECK(os.is_equal("bark"));
    }
}

} // namespace BOOST_OPENMETHOD_GENSYM

namespace BOOST_OPENMETHOD_GENSYM {

BOOST_OPENMETHOD(
    poke, (const shared_virtual_ptr<Animal>&, std::ostream&), void);

BOOST_OPENMETHOD_OVERRIDE(
    poke, (const shared_virtual_ptr<Dog>&, std::ostream& os), void) {
    os << "bark";
}

BOOST_AUTO_TEST_CASE(test_virtual_shared_by_const_reference) {
    boost::openmethod::initialize();

    {
        boost::test_tools::output_test_stream os;
        shared_virtual_ptr<Animal> animal = make_shared_virtual<Dog>();
        poke(animal, os);
        BOOST_CHECK(os.is_equal("bark"));
    }
}

} // namespace BOOST_OPENMETHOD_GENSYM

namespace BOOST_OPENMETHOD_GENSYM {

BOOST_OPENMETHOD(poke, (unique_virtual_ptr<Animal>, std::ostream&), void);

BOOST_OPENMETHOD_OVERRIDE(
    poke, (unique_virtual_ptr<Dog>, std::ostream& os), void) {
    os << "bark";
}

BOOST_AUTO_TEST_CASE(test_virtual_unique) {
    boost::openmethod::initialize();

    {
        boost::test_tools::output_test_stream os;
        poke(make_unique_virtual<Dog>(), os);
        BOOST_CHECK(os.is_equal("bark"));
    }
}

} // namespace BOOST_OPENMETHOD_GENSYM
} // namespace using_polymorphic_classes

namespace using_non_polymorphic_classes {

struct Animal {};

struct Dog : Animal {};

BOOST_OPENMETHOD_CLASSES(Animal, Dog);

BOOST_OPENMETHOD(poke, (virtual_ptr<Animal>, std::ostream&), void);

BOOST_OPENMETHOD_OVERRIDE(poke, (virtual_ptr<Dog>, std::ostream& os), void) {
    os << "bark";
}

BOOST_AUTO_TEST_CASE(test_virtual_ptr_non_polymorphic) {
    boost::openmethod::initialize();

    {
        boost::test_tools::output_test_stream os;
        Dog dog;
        auto vptr = virtual_ptr<Dog>::final(dog);
        poke(vptr, os);
        BOOST_CHECK(os.is_equal("bark"));
    }
}

} // namespace using_non_polymorphic_classes

struct Player {
    virtual ~Player() {
    }
};

struct Warrior : Player {};
struct Wizard : Player {};

struct Bear : Player {};

struct Object {
    virtual ~Object() {
    }
};

struct Axe : Object {};

template<class VirtualBearPtr>
auto poke_bear(VirtualBearPtr) {
    return std::string("growl");
}

template<class VirtualWarriorPtr, class VirtualAxePtr, class VirtualBearPtr>
auto fight_bear(VirtualWarriorPtr, VirtualAxePtr, VirtualBearPtr) {
    return "kill bear";
}

template<int N>
struct indirect_test_registry
    : test_registry_<N>::template with<policies::indirect_vptr> {};

template<int N>
using policy_types =
    boost::mp11::mp_list<test_registry_<N>, indirect_test_registry<N>>;

struct BOOST_OPENMETHOD_ID(poke);
struct BOOST_OPENMETHOD_ID(fight);

namespace BOOST_OPENMETHOD_GENSYM {

BOOST_AUTO_TEST_CASE_TEMPLATE(
    test_virtual_ptr, Registry, policy_types<__COUNTER__>) {

    BOOST_OPENMETHOD_REGISTER(
        use_classes<Player, Warrior, Object, Axe, Bear, Registry>);
    using poke = method<
        BOOST_OPENMETHOD_ID(poke),
        auto(virtual_ptr<Player, Registry>)->std::string, Registry>;
    BOOST_OPENMETHOD_REGISTER(typename poke::template override<
                              poke_bear<virtual_ptr<Player, Registry>>>);

    initialize<Registry>();

    using vptr_player = virtual_ptr<Player, Registry>;
    static_assert(detail::is_virtual_ptr<vptr_player>);
    using vptr_bear = virtual_ptr<Bear, Registry>;

    Player player;
    auto virtual_player = vptr_player::final(player);
    BOOST_TEST(&*virtual_player == &player);
    BOOST_TEST(
        (virtual_player.vptr() == Registry::template static_vptr<Player>));

    Bear bear;
    BOOST_TEST((&*vptr_bear::final(bear)) == &bear);
    BOOST_TEST((
        vptr_bear::final(bear).vptr() == Registry::template static_vptr<Bear>));

    BOOST_TEST(
        (vptr_player(bear).vptr() == Registry::template static_vptr<Bear>));

    vptr_bear virtual_bear_ptr(bear);

    struct upcast {
        static void fn(vptr_player) {
        }
    };

    upcast::fn(virtual_bear_ptr);
}
} // namespace BOOST_OPENMETHOD_GENSYM

namespace test_virtual_ptr_dispatch {

BOOST_AUTO_TEST_CASE_TEMPLATE(
    test_virtual_ptr_dispatch, Registry, policy_types<__COUNTER__>) {

    BOOST_OPENMETHOD_REGISTER(
        use_classes<Player, Warrior, Object, Axe, Bear, Registry>);

    using poke = method<
        BOOST_OPENMETHOD_ID(poke),
        auto(virtual_ptr<Player, Registry>)->std::string, Registry>;
    BOOST_OPENMETHOD_REGISTER(typename poke::template override<
                              poke_bear<virtual_ptr<Player, Registry>>>);

    using fight = method<
        BOOST_OPENMETHOD_ID(fight),
        auto(
            virtual_ptr<Player, Registry>, virtual_ptr<Object, Registry>,
            virtual_ptr<Player, Registry>)
            ->std::string,
        Registry>;
    BOOST_OPENMETHOD_REGISTER(
        typename fight::template override<fight_bear<
            virtual_ptr<Player, Registry>, virtual_ptr<Object, Registry>,
            virtual_ptr<Player, Registry>>>);

    initialize<Registry>();

    Bear bear;
    BOOST_TEST(poke::fn(virtual_ptr<Player, Registry>(bear)) == "growl");

    Warrior warrior;
    Axe axe;
    BOOST_TEST(
        fight::fn(
            virtual_ptr<Player, Registry>(warrior),
            virtual_ptr<Object, Registry>(axe),
            virtual_ptr<Player, Registry>(bear)) == "kill bear");
}

} // namespace test_virtual_ptr_dispatch

namespace test_shared_virtual_ptr_dispatch {

BOOST_AUTO_TEST_CASE_TEMPLATE(
    test_virtual_ptr_dispatch, Registry, policy_types<__COUNTER__>) {

    BOOST_OPENMETHOD_REGISTER(
        use_classes<Player, Warrior, Object, Axe, Bear, Registry>);

    using poke = method<
        BOOST_OPENMETHOD_ID(poke),
        auto(shared_virtual_ptr<Player, Registry>)->std::string, Registry>;

    BOOST_OPENMETHOD_REGISTER(typename poke::template override<
                              poke_bear<shared_virtual_ptr<Player, Registry>>>);

    using fight = method<
        BOOST_OPENMETHOD_ID(fight),
        auto(
            shared_virtual_ptr<Player, Registry>,
            shared_virtual_ptr<Object, Registry>,
            shared_virtual_ptr<Player, Registry>)
            ->std::string,
        Registry>;

    BOOST_OPENMETHOD_REGISTER(typename fight::template override<fight_bear<
                                  shared_virtual_ptr<Player, Registry>,
                                  shared_virtual_ptr<Object, Registry>,
                                  shared_virtual_ptr<Player, Registry>>>);

    initialize<Registry>();

    auto bear = make_shared_virtual<Bear, Registry>();
    auto warrior = make_shared_virtual<Warrior, Registry>();
    auto axe = make_shared_virtual<Axe, Registry>();

    BOOST_TEST(poke::fn(bear) == "growl");

    BOOST_TEST(fight::fn(warrior, axe, bear) == "kill bear");
}

} // namespace test_shared_virtual_ptr_dispatch
