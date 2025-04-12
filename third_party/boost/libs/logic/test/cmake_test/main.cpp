// Copyright Douglas Gregor 2002-2003. Use, modification and
// distribution is subject to the Boost Software License, Version
// 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/logic/tribool.hpp>

int main()
{
  using namespace boost::logic;

  tribool x; // false
  tribool y(true); // true
  tribool z(indeterminate); // indeterminate

#if defined( BOOST_NO_CXX11_EXPLICIT_CONVERSION_OPERATORS )
  // c++03 allows for implicit conversion to bool
  // c++11 uses an explicit conversion operator so this would not compile
  //       and that is tested in the compile-fail/implicit.cpp file
  // so we check the conversion to ensure it is sane
  bool bx = x;
  assert(bx == false);
  bool by = y;
  assert(by == true);
  bool bz = z;
  assert(bz == false);
#endif

  return 0;
}
