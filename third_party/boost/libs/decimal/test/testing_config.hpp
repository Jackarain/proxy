// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_TESTING_CONFIG_HPP
#define BOOST_DECIMAL_TESTING_CONFIG_HPP

#if defined(__clang__)
#  if defined __has_feature
#    if __has_feature(thread_sanitizer) || __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)
#      define BOOST_DECIMAL_REDUCE_TEST_DEPTH
#    endif
#  endif
#elif defined(__GNUC__)
#  if defined(__SANITIZE_THREAD__) || defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
#    define BOOST_DECIMAL_REDUCE_TEST_DEPTH
#  endif
#elif defined(_MSC_VER)
#  if defined(_DEBUG) || defined(__SANITIZE_ADDRESS__)
#    define BOOST_DECIMAL_REDUCE_TEST_DEPTH
#  endif
#endif

#if !defined(BOOST_DECIMAL_REDUCE_TEST_DEPTH) && ((defined(UBSAN) && (UBSAN == 1)))
#  define BOOST_DECIMAL_REDUCE_TEST_DEPTH
#endif

#endif // BOOST_DECIMAL_TESTING_CONFIG_HPP
