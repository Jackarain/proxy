// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "roles.hpp"
#include <boost/openmethod.hpp>

// tag::content[]
namespace sales {

BOOST_OPENMETHOD_OVERRIDE(
    pay, (boost::openmethod::virtual_ptr<const Salesman> emp), double) {
    return employees::BOOST_OPENMETHOD_OVERRIDER(
               pay, (boost::openmethod::virtual_ptr<const employees::Employee> emp),
               double)::fn(emp) +
        emp->sales * 0.05; // base + commission
}

BOOST_OPENMETHOD_CLASSES(employees::Employee, Salesman)

} // namespace sales
// end::content[]
