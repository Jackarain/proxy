// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_TEST_MODULE openmethod
#include <boost/test/unit_test.hpp>

#include <boost/openmethod.hpp>
#include <boost/openmethod/initialize.hpp>

#include "lib.hpp"

using namespace boost::openmethod;

BOOST_AUTO_TEST_CASE(mix_release_debug) {
    default_registry::error_handler::set(
        [](const default_registry::error_handler::error_variant& error) {
            std::visit([](auto&& arg) { throw arg; }, error);
        });

    BOOST_CHECK_THROW(initialize(), odr_violation);
}
