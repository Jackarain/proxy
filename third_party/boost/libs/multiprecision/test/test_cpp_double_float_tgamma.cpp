///////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2021 - 2025.
//  Copyright 2021 - 2025 John Maddock.
//  Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt
//

#include <boost/detail/lightweight_test.hpp>
#include <boost/math/special_functions/gamma.hpp>
#include "test.hpp"

#include <boost/multiprecision/cpp_double_fp.hpp>

namespace local
{
  template<typename T> constexpr T (max)(T a, T b) { return a > b ? a : b; }

  template<class T>
  void test()
  {
     // N[Gamma[5/2], 120]
     const T control_tgamma_2_and_half  ("1.32934038817913702047362561250585888709816209209179034616035584238968346344327413603121299255390849906217011771821192800");

     // N[Gamma[150/2], 120]
     const T control_tgamma_seventy_five("3.3078854415193864122595302822125378214568325182093497117061192683541123570097156545925087232E107");

     const T tgamma_2_and_half   = boost::math::tgamma(T(T(5)   / 2));
     const T tgamma_seventy_five = boost::math::tgamma(T(T(150) / 2));

     BOOST_CHECK_CLOSE_FRACTION(tgamma_2_and_half,    control_tgamma_2_and_half,   T(std::numeric_limits<T>::epsilon() * T(1.0E6)));
     BOOST_CHECK_CLOSE_FRACTION(tgamma_seventy_five , control_tgamma_seventy_five, T(std::numeric_limits<T>::epsilon() * T(1.0E6)));
  }
}

int main()
{
   using boost::multiprecision::cpp_double_double;
   using boost::multiprecision::cpp_double_long_double;
#ifdef BOOST_HAS_FLOAT128
   using boost::multiprecision::cpp_double_float128;
#endif

   local::test<cpp_double_double>();
   local::test<cpp_double_long_double>();
#ifdef BOOST_HAS_FLOAT128
   local::test<cpp_double_float128>();
#endif

   return boost::report_errors();
}
