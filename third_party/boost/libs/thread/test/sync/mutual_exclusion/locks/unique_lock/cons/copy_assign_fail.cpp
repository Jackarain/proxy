//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// Copyright (C) 2011 Vicente J. Botet Escriba
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// <boost/thread/locks.hpp>

// template <class Mutex> class unique_lock;

// unique_lock& operator=(unique_lock const&) = delete;

#include <boost/thread/lock_types.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/core/lightweight_test.hpp>

boost::mutex g_mutex0;
boost::mutex g_mutex1;

int main()
{
  boost::unique_lock<boost::mutex> lk0(g_mutex0);
  boost::unique_lock<boost::mutex> lk1(g_mutex1);
  lk1 = lk0;
  BOOST_TEST(lk1.mutex() == &g_mutex0);
  BOOST_TEST(lk1.owns_lock() == true);
  BOOST_TEST(lk0.mutex() == 0);
  BOOST_TEST(lk0.owns_lock() == false);

}

#include "../../../../../remove_error_code_unused_warning.hpp"
