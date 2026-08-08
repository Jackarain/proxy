// Copyright Antony Polukhin, 2025-2026.
// Copyright Fedor Osetrov, 2025-2026.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

module;

#include <boost/config.hpp>
#include <boost/assert.hpp>
#include <boost/core/invoke_swap.hpp>
#include <boost/noncopyable.hpp>
#include <boost/predef/os.h>
#include <boost/throw_exception.hpp>
#include <boost/type_index/ctti_type_index.hpp>

#ifdef BOOST_DLL_USE_STD_FS
# ifndef BOOST_DLL_USE_STD_MODULE
#   include <filesystem>
#   include <system_error>
# endif
#else
#   include <boost/filesystem/path.hpp>
#   include <boost/filesystem/operations.hpp>
#   include <boost/system/system_error.hpp>
#   include <boost/system/error_code.hpp>
#endif

#if !BOOST_OS_WINDOWS
#   include <dlfcn.h>
#   include <link.h>
#endif

#ifndef BOOST_DLL_USE_STD_MODULE
#include <algorithm>
#include <type_traits>
#include <map>
#include <memory>
#include <fstream>
#include <vector>
#endif

#ifndef _MSC_VER
#include <boost/core/demangle.hpp>
#endif

#define BOOST_DLL_INTERFACE_UNIT

export module boost.dll;

#ifdef BOOST_DLL_USE_STD_MODULE
import std;
#endif

#ifdef __clang__
#   pragma clang diagnostic ignored "-Winclude-angled-in-module-purview"
#endif

#include <boost/dll.hpp>

// Experimental features
#include <boost/dll/import_class.hpp>
#include <boost/dll/import_mangled.hpp>
#include <boost/dll/smart_library.hpp>
