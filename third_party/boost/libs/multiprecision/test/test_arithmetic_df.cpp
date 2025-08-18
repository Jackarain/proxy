///////////////////////////////////////////////////////////////
//  Copyright 2012 John Maddock.
//  Copyright 2021 - 2025 Christopher Kormanyos.
//  Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt

#if defined(_MSC_VER)
#define _SCL_SECURE_NO_WARNINGS
#endif

#include <boost/multiprecision/cpp_double_fp.hpp>

#include "test_arithmetic.hpp"

int main()
{
   using boost::multiprecision::cpp_double_float;
   using boost::multiprecision::cpp_double_double;
   using boost::multiprecision::cpp_double_long_double;
#ifdef BOOST_HAS_FLOAT128
   using boost::multiprecision::cpp_double_float128;
#endif

   constexpr cpp_double_double one { 1 };

   static_assert(static_cast<int>(one) == 1, "Error:constexpr-ness of cpp_double_double constructor failed");

   test<cpp_double_float>();
   test<cpp_double_double>();
   test<cpp_double_long_double>();
#ifdef BOOST_HAS_FLOAT128
   test<cpp_double_float128>();
#endif
   return boost::report_errors();
}
