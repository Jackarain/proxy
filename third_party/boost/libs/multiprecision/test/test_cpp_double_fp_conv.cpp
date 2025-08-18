///////////////////////////////////////////////////////////////
//  Copyright 2012 John Maddock.
//  Copyright 2023 - 2025 Christopher Kormanyos.
//  Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt
//

#ifdef _MSC_VER
#define _SCL_SECURE_NO_WARNINGS
#endif

#include <boost/detail/lightweight_test.hpp>
#include <boost/array.hpp>
#include "test.hpp"

#include <boost/multiprecision/cpp_double_fp.hpp>

int main()
{
   using namespace boost::multiprecision;
   //
   // Test interconversions between different precisions:
   //
   using double_flt_type = boost::multiprecision::cpp_double_float;
   using double_dbl_type = boost::multiprecision::cpp_double_double;

   double_flt_type f1(2);
   double_dbl_type f2(3);

   double_dbl_type f3 = f1; // implicit conversion OK
   BOOST_TEST(f3 == 2);
   double_flt_type f4(f2);  // explicit conversion OK
   BOOST_TEST(f4 == 3);

   f2 = f1;
   BOOST_TEST(f2 == 2);
   f2 = 4;
   f1 = static_cast<double_flt_type>(f2);
   BOOST_TEST(f1 == 4);

   return boost::report_errors();
}
