///////////////////////////////////////////////////////////////
//  Copyright 2025 Christopher Kormanyos.
//  Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt
//

#ifndef BOOST_MULTIPRECISION_TEST_TRAITS_HPP
#define BOOST_MULTIPRECISION_TEST_TRAITS_HPP

#if defined(TEST_CPP_DOUBLE_FLOAT)
#include <boost/multiprecision/cpp_double_fp.hpp>
#endif

template <class UnknownNumericTestType>
struct has_poor_exp_range_or_precision_support
{
   static constexpr bool value { false };
};

#if defined(TEST_CPP_DOUBLE_FLOAT)

template <> struct has_poor_exp_range_or_precision_support<::boost::multiprecision::cpp_double_float> final { static constexpr bool value { true }; };
template <> struct has_poor_exp_range_or_precision_support<::boost::multiprecision::cpp_double_double> final { static constexpr bool value { true }; };
template <> struct has_poor_exp_range_or_precision_support<::boost::multiprecision::cpp_double_long_double> final { static constexpr bool value { true }; };
#if defined(BOOST_MP_CPP_DOUBLE_FP_HAS_FLOAT128) // (which is the same as BOOST_HAS_FLOAT128)
template <> struct has_poor_exp_range_or_precision_support<::boost::multiprecision::cpp_double_float128> final { static constexpr bool value { true }; };
#endif

#endif // TEST_CPP_DOUBLE_FLOAT

#endif // BOOST_MULTIPRECISION_TEST_TRAITS_HPP
