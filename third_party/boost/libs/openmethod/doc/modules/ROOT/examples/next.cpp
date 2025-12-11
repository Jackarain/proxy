// next.cpp
// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// Example taken Dylan's documentation, see
// http://opendylan.org/documentation/intro-dylan/multiple-dispatch.html

#include <iostream>

#include <boost/openmethod.hpp>
#include <boost/openmethod/initialize.hpp>

using boost::openmethod::virtual_ptr;

using namespace std;

struct Vehicle {
    virtual ~Vehicle() {
    }
};

//[ car
struct Car : Vehicle {};

struct Truck : Vehicle {};

struct Inspector {
    virtual ~Inspector() {
    }
};

struct StateInspector : Inspector {};

BOOST_OPENMETHOD_CLASSES(Vehicle, Car, Truck, Inspector, StateInspector);

BOOST_OPENMETHOD(
    inspect, (virtual_ptr<const Vehicle>, virtual_ptr<const Inspector>), void);

BOOST_OPENMETHOD_OVERRIDE(
    inspect, (virtual_ptr<const Vehicle>, virtual_ptr<const Inspector>), void) {
    cout << "Inspect vehicle.\n";
}

BOOST_OPENMETHOD_OVERRIDE(
    inspect, (virtual_ptr<const Car> v, virtual_ptr<const Inspector> i), void) {
    next(v, i);
    cout << "Inspect seat belts.\n";
}

BOOST_OPENMETHOD_OVERRIDE(
    inspect, (virtual_ptr<const Car> v, virtual_ptr<const StateInspector> i),
    void) {
    next(v, i);
    cout << "Check road tax.\n";
}

auto main() -> int {
    boost::openmethod::initialize();

    const Vehicle& vehicle1 = Car();
    const Inspector& inspector1 = StateInspector();
    const Vehicle& vehicle2 = Truck();
    const Inspector& inspector2 = Inspector();

    cout << "First inspection:\n";
    inspect(vehicle1, inspector1);

    cout << "\nSecond inspection:\n";
    inspect(vehicle2, inspector2);

    return 0;
}
