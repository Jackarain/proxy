// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// tag::content[]
#include <boost/openmethod.hpp>
#include <boost/openmethod/initialize.hpp>
#include <iostream>

// tag::classes[]
struct Role {
    virtual ~Role() = default;
};

struct Employee : Role {};
struct Salesman : Employee {};
struct Manager : Employee {};
struct Founder : Role {};

struct Expense {
    virtual ~Expense() {
    }
};

struct Public : Expense {};
struct Bus : Public {};
struct Metro : Public {};
struct Taxi : Expense {};
struct PrivateJet : Expense {};
// end::classes[]

BOOST_OPENMETHOD_CLASSES(
    Role, Employee, Manager, Founder, Expense, Public, Bus, Metro, Taxi,
    PrivateJet);

// tag::approve[]
using boost::openmethod::virtual_ptr;

BOOST_OPENMETHOD(
    approve, (virtual_ptr<const Role>, virtual_ptr<const Expense>, double),
    bool);

BOOST_OPENMETHOD_OVERRIDE(
    approve, (virtual_ptr<const Role>, virtual_ptr<const Expense>, double),
    bool) {
    return false;
}

BOOST_OPENMETHOD_OVERRIDE(
    approve, (virtual_ptr<const Employee>, virtual_ptr<const Public>, double),
    bool) {
    return true;
}

BOOST_OPENMETHOD_OVERRIDE(
    approve,
    (virtual_ptr<const Manager>, virtual_ptr<const Taxi>, double amount),
    bool) {
    return amount < 100.0;
}

BOOST_OPENMETHOD_OVERRIDE(
    approve, (virtual_ptr<const Founder>, virtual_ptr<const Expense>, double),
    bool) {
    return true;
}

// tag::main[]
int main() {
    boost::openmethod::initialize();

    Founder bill;
    Employee bob;
    Manager alice;

    Bus bus;
    Taxi taxi;
    PrivateJet jet;

    std::cout << std::boolalpha;
    std::cout << approve(bill, bus, 4.0) << "\n";        // true
    std::cout << approve(bob, bus, 4.0) << "\n";         // true
    std::cout << approve(bob, taxi, 36.0) << "\n";       // false
    std::cout << approve(alice, taxi, 36.0) << "\n";     // true
    std::cout << approve(alice, taxi, 2000.0) << "\n";   // false
    std::cout << approve(bill, jet, 120'000.0) << "\n";  // true
    std::cout << approve(bob, jet, 120'000.0) << "\n";   // false
    std::cout << approve(alice, jet, 120'000.0) << "\n"; // false
}
// end::main[]
