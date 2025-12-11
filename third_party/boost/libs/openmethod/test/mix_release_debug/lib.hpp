// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_OPENMETHOD_TEST_MIX_RELEASE_DEBUG_LIB_HPP
#define BOOST_OPENMETHOD_TEST_MIX_RELEASE_DEBUG_LIB_HPP

#include <boost/openmethod.hpp>

struct Animal {
    virtual ~Animal() {
    }
};

struct Cat : Animal {};

BOOST_OPENMETHOD(
    poke, (boost::openmethod::virtual_ptr<Animal>, std::ostream&), void);

#endif // BOOST_OPENMETHOD_TEST_MIX_RELEASE_DEBUG_LIB_HPP
