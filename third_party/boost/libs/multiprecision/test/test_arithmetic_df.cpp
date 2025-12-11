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

template<typename FloatType>
auto test_constexpr_ness() noexcept -> void
{
   using local_float_type = FloatType;

   constexpr local_float_type one { 1 };

   static_assert(one == 1, "Error: constexpr local_float_type construction failed");

   BOOST_CHECK(one == 1);

   constexpr local_float_type dd_a { 1.23 };
   constexpr local_float_type dd_b { 4.56 };
   constexpr local_float_type dd_c { dd_a * dd_b };

   constexpr local_float_type dd_s { sqrt(dd_a) };
   constexpr local_float_type sqrt_hundred { sqrt(local_float_type(100)) };
   constexpr local_float_type log_hundred { log(local_float_type(100)) };

   static_assert(dd_c > 5, "Error in constexpr multiplication");
   static_assert(dd_s > 1, "Error in constexpr square root");
   static_assert(sqrt_hundred == 10, "Error in constexpr square root");
   static_assert(log_hundred > local_float_type(4.605L), "Error in constexpr logarithm");

   BOOST_CHECK(dd_c > 5);
   BOOST_CHECK(dd_s > 1);
   BOOST_CHECK(sqrt_hundred == 10);
   BOOST_CHECK(log_hundred > local_float_type(4.605L));
}

int main()
{
   ::test_constexpr_ness<boost::multiprecision::cpp_double_float>();
   ::test_constexpr_ness<boost::multiprecision::cpp_double_double>();
   ::test_constexpr_ness<boost::multiprecision::cpp_double_long_double>();
#ifdef BOOST_HAS_FLOAT128
   ::test_constexpr_ness<boost::multiprecision::cpp_double_float128>();
#endif

   test<boost::multiprecision::cpp_double_float>();
   test<boost::multiprecision::cpp_double_double>();
   test<boost::multiprecision::cpp_double_long_double>();
#ifdef BOOST_HAS_FLOAT128
   test<boost::multiprecision::cpp_double_float128>();
#endif
   return boost::report_errors();
}
