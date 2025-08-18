// Copyright (c) 2016-2025 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// To compile manually use a command like the following:
// clang++ -I ../include -std=c++20 --precompile -x c++-module any.cppm

module;

#include <boost/assert.hpp>
#include <boost/config.hpp>
#include <boost/throw_exception.hpp>
#include <boost/type_index.hpp>

#ifdef BOOST_ANY_USE_STD_MODULE
import std;
#else
#include <memory>
#include <stdexcept>
#include <typeinfo>
#include <type_traits>
#include <utility>
#endif

#define BOOST_ANY_INTERFACE_UNIT

export module boost.any;

#ifdef __clang__
#   pragma clang diagnostic ignored "-Winclude-angled-in-module-purview"
#endif

#include <boost/any.hpp>
#include <boost/any/basic_any.hpp>
#include <boost/any/unique_any.hpp>

