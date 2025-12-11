// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// tag::content[]
// main.cpp

#include "roles.hpp"
#include <iostream>

#include <boost/openmethod/initialize.hpp>

int main() {
    boost::openmethod::initialize();

    Employee bill;
    Salesman bob; bob.sales = 100'000.0;

    std::cout << "pay bill: $" << pay(bill) << "\n"; // pay bill: $5000
    std::cout << "pay bob: $" << pay(bob) << "\n"; // pay bob: $10000
}
// end::content[]
