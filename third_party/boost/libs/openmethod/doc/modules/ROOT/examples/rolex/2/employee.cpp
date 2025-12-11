// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// tag::content[]
#include "roles.hpp"
#include <boost/openmethod.hpp>

BOOST_OPENMETHOD_DEFINE_OVERRIDER(
    pay, (boost::openmethod::virtual_ptr<const Employee>), double) {
    return 5000.0;
}

BOOST_OPENMETHOD_CLASSES(Employee)
// end::content[]
