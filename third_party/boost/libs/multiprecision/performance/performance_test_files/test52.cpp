///////////////////////////////////////////////////////////////
//  Copyright John Maddock 2019 - 2025.
//  Copyright Christopher Kormanyos 2021 - 2025.
//  Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt

#include "../performance_test.hpp"
#if defined(TEST_CPP_DOUBLE_FLOAT)
#include <boost/multiprecision/cpp_double_fp.hpp>
#endif

void test52()
{
#ifdef TEST_CPP_DOUBLE_FLOAT
   using boost::multiprecision::cpp_double_double;

   test<cpp_double_double>("cpp_double_fp_backend<double>", std::numeric_limits<cpp_double_double>::digits10);
#endif
}
