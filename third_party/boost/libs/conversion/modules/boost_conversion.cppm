// Copyright (c) 2025-2026 Antony Polukhin
// Copyright (c) 2025-2026 Fedor Osetrov
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

module;

#include <boost/config.hpp>
#include <boost/throw_exception.hpp>
#include <boost/assert.hpp>
#include <boost/pointer_cast.hpp>

#ifndef BOOST_CONVERSION_USE_STD_MODULE
#include <memory>
#include <typeinfo>
#include <type_traits>
#endif

#define BOOST_CONVERSION_INTERFACE_UNIT

export module boost.conversion;

#ifdef BOOST_CONVERSION_USE_STD_MODULE
import std;
#endif

#ifdef __clang__
#   pragma clang diagnostic ignored "-Winclude-angled-in-module-purview"
#endif

#include <boost/implicit_cast.hpp>
#include <boost/polymorphic_cast.hpp>
#include <boost/polymorphic_pointer_cast.hpp>
