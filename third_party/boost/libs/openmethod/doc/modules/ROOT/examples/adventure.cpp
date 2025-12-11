// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <memory>
#include <string>

#include <boost/openmethod.hpp>
#include <boost/openmethod/initialize.hpp>

using boost::openmethod::virtual_ptr;

struct Character {
    virtual ~Character() {
    }
};

struct Warrior : Character {};

struct Device {
    virtual ~Device() {
    }
};

struct Hands : Device {};
struct Axe : Device {};
struct Banana : Device {};

struct Creature {
    virtual ~Creature() {
    }
};

struct Dragon : Creature {};
struct Bear : Creature {};

BOOST_OPENMETHOD_CLASSES(
    Character, Warrior, Device, Hands, Axe, Banana, Creature, Dragon, Bear);

BOOST_OPENMETHOD(
    fight, (virtual_ptr<Character>, virtual_ptr<Creature>, virtual_ptr<Device>),
    std::string);

BOOST_OPENMETHOD_OVERRIDE(
    fight, (virtual_ptr<Character>, virtual_ptr<Creature>, virtual_ptr<Banana>),
    std::string) {
    return "are you insane?";
}

BOOST_OPENMETHOD_OVERRIDE(
    fight, (virtual_ptr<Character>, virtual_ptr<Creature>, virtual_ptr<Axe>),
    std::string) {
    return "not agile enough to wield";
}

BOOST_OPENMETHOD_OVERRIDE(
    fight, (virtual_ptr<Warrior>, virtual_ptr<Creature>, virtual_ptr<Axe>),
    std::string) {
    return "and cuts it into pieces";
}

BOOST_OPENMETHOD_OVERRIDE(
    fight, (virtual_ptr<Warrior>, virtual_ptr<Dragon>, virtual_ptr<Axe>),
    std::string) {
    return "and dies a honorable death";
}

BOOST_OPENMETHOD_OVERRIDE(
    fight, (virtual_ptr<Character>, virtual_ptr<Dragon>, virtual_ptr<Hands>),
    std::string) {
    return "Congratulations! You have just vainquished a dragon with your bare "
           "hands"
           " (unbelievable, isn't it?)";
}

auto main() -> int {
    boost::openmethod::initialize();

    std::unique_ptr<Character> bob = std::make_unique<Character>(),
                               rambo = std::make_unique<Warrior>();

    std::unique_ptr<Creature> elliott = std::make_unique<Dragon>(),
                              paddington = std::make_unique<Bear>();

    std::unique_ptr<Device> hands = std::make_unique<Hands>(),
                            axe = std::make_unique<Axe>(),
                            chiquita = std::make_unique<Banana>();

    std::cout << "bob fights elliot with axe:\n"
              << fight(*bob, *elliott, *axe) << "\n";
    // bob fights elliot with axe:
    // not agile enough to wield

    std::cout << "rambo fights paddington with axe:\n"
              << fight(*rambo, *paddington, *axe) << "\n";
    // rambo fights paddington with axe:
    // and cuts it into pieces

    std::cout << "rambo fights paddington with banana:\n"
              << fight(*rambo, *paddington, *chiquita) << "\n";
    // rambo fights paddington with banana:
    // are you insane?

    std::cout << "rambo fights elliott with axe:\n"
              << fight(*rambo, *elliott, *axe) << "\n";
    // rambo fights elliott with axe:
    // and dies a honorable death

    std::cout << "bob fights elliot with hands:\n"
              << fight(*bob, *elliott, *hands) << "\n";
    // bob fights elliot with hands: Congratulations! You have just vainquished
    // a dragon with your bare hands (unbelievable, isn't it?)

    std::cout << "rambo fights elliot with hands:\n"
              << fight(*rambo, *elliott, *hands) << "\n";
    // rambo fights elliot with hands:
    // you just killed a dragon with your bare hands. Incredible isn't it?

    return 0;
}

auto call_fight(Character& character, Creature& creature, Device& device) {
    return fight(character, creature, device);
}
