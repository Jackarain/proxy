// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/openmethod.hpp>
#include <boost/openmethod/initialize.hpp>

#define BOOST_TEST_MODULE test_rolex
#include <boost/test/unit_test.hpp>

using boost::openmethod::virtual_ptr;

struct Role {
    virtual ~Role() {}
};

struct Employee : Role {
    virtual auto pay() -> double;
};

struct Manager : Employee {
    virtual auto pay() -> double;
};

struct Founder : Role {};

struct Expense {
    virtual ~Expense() {}
};

struct Public : Expense {};
struct Bus : Public {};
struct Metro : Public {};
struct Taxi : Expense {};
struct PrivateJet : Expense {};

BOOST_OPENMETHOD_CLASSES(
    Role, Employee, Manager, Founder, Expense, Public, Bus, Metro, Taxi,
    PrivateJet);

BOOST_OPENMETHOD(pay, (virtual_ptr<Employee>), double);

BOOST_OPENMETHOD(
    approve, (virtual_ptr<const Role>, virtual_ptr<const Expense>, double),
    bool);

BOOST_OPENMETHOD_OVERRIDE(pay, (virtual_ptr<Employee>), double) {
    return 3000;
}

BOOST_OPENMETHOD_OVERRIDE(pay, (virtual_ptr<Manager> exec), double) {
    return next(exec) + 2000;
}

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
    approve, (virtual_ptr<const Manager>, virtual_ptr<const Taxi>, double),
    bool) {
    return true;
}

BOOST_OPENMETHOD_OVERRIDE(
    approve, (virtual_ptr<const Founder>, virtual_ptr<const Expense>, double),
    bool) {
    return true;
}

auto Employee::pay() -> double {
    return 3000;
}

auto Manager::pay() -> double {
    return Employee::pay() + 2000;
}

auto call_pay(Employee& emp) -> double {
    return pay(emp);
}

auto call_pay_vfunc(Employee& emp) -> double {
    return emp.pay();
}

auto call_approve(const Role& r, const Expense& e, double a) -> bool {
    return approve(r, e, a);
}

BOOST_AUTO_TEST_CASE(pay_employee) {
    boost::openmethod::initialize();
    Employee e;
    BOOST_TEST(pay(e) == 3000);
    BOOST_TEST(call_pay(e) == 3000);
    BOOST_TEST(call_pay_vfunc(e) == 3000);
}

BOOST_AUTO_TEST_CASE(pay_manager_calls_next) {
    boost::openmethod::initialize();
    Manager m;
    BOOST_TEST(pay(m) == 5000);
    BOOST_TEST(call_pay(m) == 5000);
    BOOST_TEST(call_pay_vfunc(m) == 5000);
}

BOOST_AUTO_TEST_CASE(approve_employee) {
    boost::openmethod::initialize();
    Employee e;
    Bus bus;
    Metro metro;
    Taxi taxi;
    PrivateJet jet;
    // Employee may take public transport
    BOOST_TEST(approve(e, bus, 10) == true);
    BOOST_TEST(approve(e, metro, 10) == true);
    // Employee may not take taxi or private jet
    BOOST_TEST(approve(e, taxi, 10) == false);
    BOOST_TEST(approve(e, jet, 10) == false);
}

BOOST_AUTO_TEST_CASE(approve_manager) {
    boost::openmethod::initialize();
    Manager m;
    Bus bus;
    Metro metro;
    Taxi taxi;
    PrivateJet jet;
    // Manager inherits employee's public transport approval
    BOOST_TEST(approve(m, bus, 10) == true);
    BOOST_TEST(approve(m, metro, 10) == true);
    // Manager may also take a taxi
    BOOST_TEST(approve(m, taxi, 10) == true);
    // Manager may not take a private jet
    BOOST_TEST(approve(m, jet, 10) == false);
}

BOOST_AUTO_TEST_CASE(approve_founder) {
    boost::openmethod::initialize();
    Founder f;
    Bus bus;
    Metro metro;
    Taxi taxi;
    PrivateJet jet;
    // Founder approves all expenses
    BOOST_TEST(approve(f, bus, 10) == true);
    BOOST_TEST(approve(f, metro, 10) == true);
    BOOST_TEST(approve(f, taxi, 10) == true);
    BOOST_TEST(approve(f, jet, 10) == true);
}

BOOST_AUTO_TEST_CASE(approve_base_role_denied) {
    boost::openmethod::initialize();
    Role r;
    Bus bus;
    Taxi taxi;
    PrivateJet jet;
    // Base Role catches all — nothing approved
    BOOST_TEST(approve(r, bus, 10) == false);
    BOOST_TEST(approve(r, taxi, 10) == false);
    BOOST_TEST(approve(r, jet, 10) == false);
}

BOOST_AUTO_TEST_CASE(approve_via_wrapper) {
    boost::openmethod::initialize();
    Employee e;
    Manager m;
    Founder f;
    Bus bus;
    Taxi taxi;
    BOOST_TEST(call_approve(e, bus, 10) == true);
    BOOST_TEST(call_approve(e, taxi, 10) == false);
    BOOST_TEST(call_approve(m, taxi, 10) == true);
    BOOST_TEST(call_approve(f, taxi, 10) == true);
}
