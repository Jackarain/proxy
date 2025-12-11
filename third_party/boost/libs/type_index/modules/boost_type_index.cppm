// Copyright (c) 2016-2025 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// To compile manually use a command like the folowing:
// clang++ -I ../include -std=c++20 --precompile -x c++-module boost_type_index.cppm

module;

#include <version>
#include <cstddef>
#include <cstdint>

#if __has_include(<cxxabi.h>)
#  include <cxxabi.h>
#endif

#include <boost/config.hpp>
#include <boost/container_hash/hash_fwd.hpp>
#include <boost/container_hash/hash.hpp>
#include <boost/throw_exception.hpp>

#ifndef BOOST_TYPE_INDEX_USE_STD_MODULE
#include <cstring>
#include <cstdlib>
#include <iosfwd>
#include <memory>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <type_traits>
#endif

#define BOOST_TYPE_INDEX_INTERFACE_UNIT

export module boost.type_index;

#ifdef BOOST_TYPE_INDEX_USE_STD_MODULE
// Should not be in the global module fragment
// https://eel.is/c++draft/module#global.frag-1
import std;
#endif

#ifdef __clang__
#   pragma clang diagnostic ignored "-Winclude-angled-in-module-purview"
#endif

#include <boost/type_index.hpp>
#include <boost/type_index/ctti_type_index.hpp>
#include <boost/type_index/runtime_cast.hpp>
#include <boost/type_index/runtime_cast/boost_shared_ptr_cast.hpp>
#include <boost/type_index/runtime_cast/pointer_cast.hpp>
#include <boost/type_index/runtime_cast/reference_cast.hpp>
#include <boost/type_index/runtime_cast/register_runtime_class.hpp>
#include <boost/type_index/runtime_cast/std_shared_ptr_cast.hpp>
#include <boost/type_index/stl_type_index.hpp>
#include <boost/type_index/type_index_facade.hpp>

