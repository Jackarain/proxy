// Copyright 2015 Ion Gaztanaga
// Copyright 2026 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

// BOOST_ENABLE_ASSERT_HANDLER, BOOST_ENABLE_ASSERT_DEBUG_HANDLER

#define BOOST_ENABLE_ASSERT_HANDLER
#define BOOST_ENABLE_ASSERT_DEBUG_HANDLER
#include <boost/assert.hpp>

#if defined(NDEBUG)

#ifndef BOOST_ASSERT_IS_VOID
#error "BOOST_ASSERT should be void if BOOST_ENABLE_ASSERT_HANDLER, BOOST_ENABLE_ASSERT_DEBUG_HANDLER, and NDEBUG are defined"
#endif

#else

#ifdef BOOST_ASSERT_IS_VOID
#error "BOOST_ASSERT should NOT be void if BOOST_ENABLE_ASSERT_HANDLER and BOOST_ENABLE_ASSERT_DEBUG_HANDLER are defined, but NDEBUG is not"
#endif

#endif

// +BOOST_DISABLE_ASSERTS

#define BOOST_DISABLE_ASSERTS
#include <boost/assert.hpp>

#ifndef BOOST_ASSERT_IS_VOID
#error "Error: BOOST_ASSERT should always be void with BOOST_DISABLE_ASSERTS"
#endif

int main()
{
}
