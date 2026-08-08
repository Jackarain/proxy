// (C) Copyright 2013 Ruslan Baratov
// Copyright (C) 2014 Vicente Botet
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

//  See www.boost.org/libs/thread for documentation.

#define BOOST_THREAD_VERSION 4

#include <iostream> // std::cout
#include <boost/thread/scoped_thread.hpp>
#include <boost/thread/with_lock_guard.hpp>

boost::mutex g_mutex; // protection for 'g_x' and 'std::cout'
int g_x = 0;

#if defined(BOOST_NO_CXX11_LAMBDAS)  || (defined BOOST_MSVC && _MSC_VER < 1700)
void print_x() {
  ++g_x;
  std::cout << "x = " << g_x << std::endl;
}

void job() {
  for (int i = 0; i < 10; ++i) {
    boost::with_lock_guard(g_mutex, print_x);
    boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
  }
}
#else
void job() {
  for (int i = 0; i < 10; ++i) {
    boost::with_lock_guard(
        g_mutex,
        []() {
          ++g_x;
          std::cout << "x = " << g_x << std::endl;
        }
    );
    boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
  }
}
#endif

int main() {
#if defined(BOOST_NO_CXX11_LAMBDAS)  || (defined BOOST_MSVC && _MSC_VER < 1700)
  std::cout << "(no lambdas)" << std::endl;
#endif
  boost::scoped_thread<> thread_1((boost::thread(job)));
  boost::scoped_thread<> thread_2((boost::thread(job)));
  boost::scoped_thread<> thread_3((boost::thread(job)));
  return 0;
}
