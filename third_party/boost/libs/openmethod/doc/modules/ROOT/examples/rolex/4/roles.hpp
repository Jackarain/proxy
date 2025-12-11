// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// tag::content[]
// roles.hpp

#ifndef ROLES_HPP
#define ROLES_HPP

#include <boost/openmethod.hpp>

namespace employees {

struct Employee {
    virtual ~Employee() = default;
};

BOOST_OPENMETHOD(pay, (boost::openmethod::virtual_ptr<const Employee>), double);

BOOST_OPENMETHOD_INLINE_OVERRIDE(
    pay, (boost::openmethod::virtual_ptr<const Employee>), double) {
    return 5000.0;
}

}

namespace sales {

struct Salesman : employees::Employee {
    double sales = 0.0;
};

} // namespace sales
#endif // ROLES_HPP
// end::content[]
