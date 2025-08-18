///////////////////////////////////////////////////////////////
//  Copyright John Maddock 2019 - 2025.
//  Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt

#include "../performance_test.hpp"
#if defined(TEST_FLOAT128)
#include <boost/multiprecision/float128.hpp>
#endif

void test53()
{
#ifdef TEST_FLOAT128
   using boost::multiprecision::float128;

   test<float128>("boost::multiprecision::float128", std::numeric_limits<boost::multiprecision::float128>::digits10);
#endif
}
