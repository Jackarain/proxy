// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// tag::content[]
// roles.hpp

#ifndef ROLES_HPP
#define ROLES_HPP

#include <boost/openmethod.hpp>

struct Employee { virtual ~Employee() = default; };

struct Salesman : Employee {
    double sales = 0.0;
};

BOOST_OPENMETHOD(pay, (boost::openmethod::virtual_ptr<const Employee>), double);

#endif // ROLES_HPP
// end::content[]
