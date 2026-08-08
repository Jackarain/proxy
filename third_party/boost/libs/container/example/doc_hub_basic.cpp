/* Basic example of use of boost::container::hub.
 * 
 * Copyright 2026 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 */

#include <boost/config.hpp>

#if BOOST_CXX_VERSION < 201103L

int main() { return 0; }

#else

#include <boost/container/hub.hpp>
#ifdef NDEBUG
#	undef NDEBUG
#endif
#include <cassert>

int main()
{
  boost::container::hub<int> h;

  // Insert some elements and keep an iterator to one of them
  for(int i = 0; i < 100; ++i) h.insert(i);
  auto it = h.insert(100);
  for(int i = 101; i < 200; ++i) h.insert(i);

  // Erase some of the elements
  erase_if(h, [](int x) { return x % 2 != 0;});
  assert(*it == 100); // iterator still valid

  // Insert many more elements
  for(int i = 200; i < 10000; ++i) h.insert(i);
  assert(*it == 100); // iterator still valid
}

#endif
