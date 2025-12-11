// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/openmethod.hpp>
#include <boost/openmethod/initialize.hpp>

using boost::openmethod::virtual_ptr;

struct Role {
    virtual ~Role() {
    }
};

struct Employee : Role {
    virtual auto pay() -> double;
};

struct Manager : Employee {
    virtual auto pay() -> double;
};

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

auto main() -> int {
    boost::openmethod::initialize();
}

auto call_pay(Employee& emp) -> double {
    return pay(emp);
}

auto Employee::pay() -> double {
    return 3000;
}

auto Manager::pay() -> double {
    return Employee::pay() + 2000;
}

auto call_pay_vfunc(Employee& emp) -> double {
    return emp.pay();
}

auto call_approve(const Role& r, const Expense& e, double a) -> bool {
    return approve(r, e, a);
}
