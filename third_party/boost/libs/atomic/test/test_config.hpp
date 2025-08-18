//  Copyright (c) 2025 Andrey Semashev
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_ATOMIC_TEST_TEST_CONFIG_HPP_INCLUDED_
#define BOOST_ATOMIC_TEST_TEST_CONFIG_HPP_INCLUDED_

#include <boost/config.hpp>

#if defined(TSAN) && defined(__GNUC__)

// Suppresses TSAN for a marked function
#define BOOST_ATOMIC_TEST_NO_SANITIZE_THREAD __attribute__((no_sanitize("thread")))

#else // defined(TSAN) && defined(__GNUC__)

#define BOOST_ATOMIC_TEST_NO_SANITIZE_THREAD

#endif // defined(TSAN) && defined(__GNUC__)

#endif // BOOST_ATOMIC_TEST_TEST_CONFIG_HPP_INCLUDED_
