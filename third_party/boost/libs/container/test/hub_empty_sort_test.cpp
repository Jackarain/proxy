/* Copyright 2026 Joaquin M Lopez Munoz.
 * Copyright 2026 Ion Gaztanaga.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 */

#include <boost/config.hpp>

#if BOOST_CXX_VERSION < 201103L

int main() { return 0; }

#else

#include <boost/container/hub.hpp>
#include <boost/core/lightweight_test.hpp>


int main()
{
  /* Warning reported in 
   * https://lists.boost.org/archives/list/boost@lists.boost.org/
   * message/ZTL2D2UMIHAAC6ZLHLH4Y3XETTQXAHBZ/ .
   */
  boost::container::hub<int> h{};
  h.sort();
  BOOST_TEST(h.empty());

  return boost::report_errors();
}

#endif
