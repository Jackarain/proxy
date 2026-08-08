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

// <boost/thread/null_mutex.hpp>

// class null_mutex;

// void lock();

#include <boost/thread/null_mutex.hpp>
#include <boost/thread/thread.hpp>
#include <boost/core/lightweight_test.hpp>
#include "../../../timming.hpp"

boost::null_mutex g_mutex;

#if defined BOOST_THREAD_USES_CHRONO
typedef boost::chrono::high_resolution_clock Clock;
typedef Clock::time_point time_point;
typedef Clock::duration duration;
typedef boost::chrono::milliseconds ms;
typedef boost::chrono::nanoseconds ns;
#else
#endif

const ms max_diff(BOOST_THREAD_TEST_TIME_MS);

void f()
{
#if defined BOOST_THREAD_USES_CHRONO
  time_point t0 = Clock::now();
  g_mutex.lock();
  time_point t1 = Clock::now();
  g_mutex.lock();
  g_mutex.unlock();
  g_mutex.unlock();
  ns d = t1 - t0 ;
  BOOST_THREAD_TEST_IT(d, ns(max_diff));
#else
  //time_point t0 = Clock::now();
  g_mutex.lock();
  //time_point t1 = Clock::now();
  g_mutex.lock();
  g_mutex.unlock();
  g_mutex.unlock();
  //ns d = t1 - t0 ;
  //BOOST_TEST(d < max_diff);
#endif
}

int main()
{
  g_mutex.lock();
  boost::thread t(f);
  g_mutex.unlock();
  t.join();

  return boost::report_errors();
}


