///////////////////////////////////////////////////////////////
//  Copyright John Maddock 2011 - 2025.
//  Copyright Christopher Kormanyos 2023 - 2025.
//  Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt

#ifdef _MSC_VER
#define _SCL_SECURE_NO_WARNINGS
#endif

#include "test.hpp"

#if !defined(TEST_MPF_50) && !defined(TEST_MPF) && !defined(TEST_BACKEND) && !defined(TEST_MPZ) &&         \
    !defined(TEST_CPP_DEC_FLOAT) && !defined(TEST_MPFR) && !defined(TEST_MPFR_50) && !defined(TEST_MPQ) && \
    !defined(TEST_TOMMATH) && !defined(TEST_CPP_INT) && !defined(TEST_MPFI_50) && !defined(TEST_FLOAT128) && \
    !defined(TEST_CPP_BIN_FLOAT) && !defined(TEST_CPP_DOUBLE_FLOAT)
#define TEST_MPF_50
#define TEST_MPF
#define TEST_BACKEND
#define TEST_MPZ
#define TEST_MPFR
#define TEST_MPFR_50
#define TEST_CPP_DEC_FLOAT
#define TEST_MPQ
#define TEST_TOMMATH
#define TEST_CPP_INT
#define TEST_MPFI_50
#define TEST_FLOAT128
#define TEST_CPP_BIN_FLOAT
#define TEST_CPP_DOUBLE_FLOAT

#ifdef _MSC_VER
#pragma message("CAUTION!!: No backend type specified so testing everything.... this will take some time!!")
#endif
#ifdef __GNUC__
#pragma warning "CAUTION!!: No backend type specified so testing everything.... this will take some time!!"
#endif

#endif

#include <test_traits.hpp> // Note: include this AFTER the test-backends are defined

#if defined(TEST_MPF_50) || defined(TEST_MPF) || defined(TEST_MPZ) || defined(TEST_MPQ)
#include <boost/multiprecision/gmp.hpp>
#endif
#ifdef TEST_BACKEND
#include <boost/multiprecision/concepts/mp_number_archetypes.hpp>
#endif
#ifdef TEST_CPP_DEC_FLOAT
#include <boost/multiprecision/cpp_dec_float.hpp>
#endif
#if defined(TEST_MPFR) || defined(TEST_MPFR_50)
#include <boost/multiprecision/mpfr.hpp>
#endif
#if defined(TEST_MPFI_50)
#include <boost/multiprecision/mpfi.hpp>
#endif
#ifdef TEST_TOMMATH
#include <boost/multiprecision/tommath.hpp>
#endif
#ifdef TEST_CPP_INT
#include <boost/multiprecision/cpp_int.hpp>
#endif
#ifdef TEST_FLOAT128
#include <boost/multiprecision/float128.hpp>
#endif
#ifdef TEST_CPP_BIN_FLOAT
#include <boost/multiprecision/cpp_bin_float.hpp>
#endif
#if defined(TEST_CPP_DOUBLE_FLOAT)
#include <boost/multiprecision/cpp_double_fp.hpp>
#endif

#ifdef BOOST_MSVC
#pragma warning(disable : 4127)
#pragma warning(disable : 4723)
#endif

#define PRINT(x) \
   std::cout << BOOST_STRINGIZE(x) << " = " << std::numeric_limits<Number>::x << std::endl;

template <class Number>
void test_specific(const std::integral_constant<int, boost::multiprecision::number_kind_floating_point>&)
{
   Number minv, maxv;
   minv = (std::numeric_limits<Number>::min)();
   maxv = (std::numeric_limits<Number>::max)();
   BOOST_CHECK((boost::math::isnormal)(minv));
   BOOST_CHECK((boost::math::isnormal)(maxv));
   BOOST_CHECK((boost::math::isnormal)(log(minv)));
   BOOST_CHECK((boost::math::isnormal)(log(maxv)));
   BOOST_CHECK((boost::math::isnormal)(sqrt(minv)));
   BOOST_CHECK((boost::math::isnormal)(sqrt(maxv)));

   BOOST_IF_CONSTEXPR(std::numeric_limits<Number>::is_specialized)
   {
      BOOST_IF_CONSTEXPR(std::numeric_limits<Number>::has_quiet_NaN)
      {
         BOOST_TEST((boost::math::isnan)(std::numeric_limits<Number>::quiet_NaN()));
         BOOST_TEST(FP_NAN == (boost::math::fpclassify)(std::numeric_limits<Number>::quiet_NaN()));
         BOOST_TEST(!(boost::math::isfinite)(std::numeric_limits<Number>::quiet_NaN()));
         BOOST_TEST(!(boost::math::isnormal)(std::numeric_limits<Number>::quiet_NaN()));
         BOOST_TEST(!(boost::math::isinf)(std::numeric_limits<Number>::quiet_NaN()));
      }
      BOOST_IF_CONSTEXPR(std::numeric_limits<Number>::has_signaling_NaN)
      {
         BOOST_TEST((boost::math::isnan)(std::numeric_limits<Number>::signaling_NaN()));
         BOOST_TEST(FP_NAN == (boost::math::fpclassify)(std::numeric_limits<Number>::signaling_NaN()));
         BOOST_TEST(!(boost::math::isfinite)(std::numeric_limits<Number>::signaling_NaN()));
         BOOST_TEST(!(boost::math::isnormal)(std::numeric_limits<Number>::signaling_NaN()));
         BOOST_TEST(!(boost::math::isinf)(std::numeric_limits<Number>::signaling_NaN()));
      }
      BOOST_IF_CONSTEXPR(std::numeric_limits<Number>::has_infinity)
      {
         BOOST_TEST((boost::math::isinf)(std::numeric_limits<Number>::infinity()));
         BOOST_TEST(FP_INFINITE == (boost::math::fpclassify)(std::numeric_limits<Number>::infinity()));
         BOOST_TEST(!(boost::math::isfinite)(std::numeric_limits<Number>::infinity()));
         BOOST_TEST(!(boost::math::isnormal)(std::numeric_limits<Number>::infinity()));
         BOOST_TEST(!(boost::math::isnan)(std::numeric_limits<Number>::infinity()));
      }
      BOOST_IF_CONSTEXPR(std::numeric_limits<Number>::has_denorm == std::denorm_present)
      {
         BOOST_TEST(FP_SUBNORMAL == (boost::math::fpclassify)(std::numeric_limits<Number>::denorm_min()));
         BOOST_TEST(FP_SUBNORMAL == (boost::math::fpclassify)((std::numeric_limits<Number>::min)() / 2));
         BOOST_TEST((boost::math::isfinite)(std::numeric_limits<Number>::denorm_min()));
         BOOST_TEST(!(boost::math::isnormal)(std::numeric_limits<Number>::denorm_min()));
         BOOST_TEST(!(boost::math::isinf)(std::numeric_limits<Number>::denorm_min()));
         BOOST_TEST(!(boost::math::isnan)(std::numeric_limits<Number>::denorm_min()));
         BOOST_TEST(0 == std::numeric_limits<Number>::denorm_min() / 2);
         BOOST_TEST(0 != (std::numeric_limits<Number>::min)() / 2);
         BOOST_TEST(0 != std::numeric_limits<Number>::denorm_min());
      }
      BOOST_IF_CONSTEXPR(std::numeric_limits<Number>::has_denorm == std::denorm_absent)
      {
         BOOST_TEST(std::numeric_limits<Number>::denorm_min() > 0);
         BOOST_TEST(!(std::numeric_limits<Number>::denorm_min() > (std::numeric_limits<Number>::min)()));
         BOOST_TEST(!(std::numeric_limits<Number>::denorm_min() < (std::numeric_limits<Number>::min)()));
         BOOST_TEST(std::numeric_limits<Number>::denorm_min() == (std::numeric_limits<Number>::min)());
         BOOST_TEST(FP_NORMAL == (boost::math::fpclassify)(std::numeric_limits<Number>::denorm_min()));
         BOOST_TEST(FP_NORMAL == (boost::math::fpclassify)(-std::numeric_limits<Number>::denorm_min()));
         BOOST_TEST(FP_ZERO != (boost::math::fpclassify)(std::numeric_limits<Number>::denorm_min()));
      }
      #if !defined(TEST_BACKEND)
      BOOST_IF_CONSTEXPR(boost::multiprecision::number_category<Number>::value == boost::multiprecision::number_kind_floating_point)
      {
         BOOST_TEST(FP_NORMAL == (boost::math::fpclassify)(std::numeric_limits<Number>::lowest()));
         BOOST_TEST(std::numeric_limits<Number>::lowest() < Number { 0 });
         BOOST_IF_CONSTEXPR(std::numeric_limits<Number>::max_exponent > std::numeric_limits<float>::max_exponent)
         {
            BOOST_TEST(std::numeric_limits<Number>::lowest() < Number { std::numeric_limits<float>::lowest() });
         }
         BOOST_IF_CONSTEXPR(std::numeric_limits<Number>::max_exponent > std::numeric_limits<double>::max_exponent)
         {
            BOOST_TEST(std::numeric_limits<Number>::lowest() < Number { std::numeric_limits<double>::lowest() });
         }
         BOOST_IF_CONSTEXPR(std::numeric_limits<Number>::max_exponent > std::numeric_limits<long double>::max_exponent)
         {
            BOOST_TEST(std::numeric_limits<Number>::lowest() < Number { std::numeric_limits<long double>::lowest() });
         }
      }
      #endif
   }
   Number n = 0;
   BOOST_TEST((boost::math::fpclassify)(n) == FP_ZERO);
   BOOST_TEST((boost::math::isfinite)(n));
   BOOST_TEST(!(boost::math::isnormal)(n));
   BOOST_TEST(!(boost::math::isinf)(n));
   BOOST_TEST(!(boost::math::isnan)(n));
   n = 2;
   BOOST_TEST((boost::math::fpclassify)(n) == FP_NORMAL);
   BOOST_TEST((boost::math::isfinite)(n));
   BOOST_TEST((boost::math::isnormal)(n));
   BOOST_TEST(!(boost::math::isinf)(n));
   BOOST_TEST(!(boost::math::isnan)(n));

   BOOST_IF_CONSTEXPR(std::numeric_limits<Number>::round_style == std::round_to_nearest)
   {
      BOOST_CHECK_EQUAL(std::numeric_limits<Number>::round_error(), 0.5);
   }
   else BOOST_IF_CONSTEXPR(std::numeric_limits<Number>::round_style != std::round_indeterminate)
   {
      // Round error is 1.0:
      BOOST_CHECK_EQUAL(std::numeric_limits<Number>::round_error(), 1);
   }
   else
   {
      // Round error is presumably somewhere between 0.5 and 1:
      BOOST_CHECK((std::numeric_limits<Number>::round_error() <= 1) && (std::numeric_limits<Number>::round_error() >= 0.5));
   }
}

template <class Number>
void test_specific(const std::integral_constant<int, boost::multiprecision::number_kind_integer>&)
{
   BOOST_IF_CONSTEXPR(std::numeric_limits<Number>::is_modulo)
   {
      BOOST_IF_CONSTEXPR(!std::numeric_limits<Number>::is_signed)
      {
         BOOST_TEST(1 + (std::numeric_limits<Number>::max)() == 0);
         BOOST_TEST(--Number(0) == (std::numeric_limits<Number>::max)());
      }
   }
}

template <class Number, class T>
void test_specific(const T&)
{
}

template <class Number>
void test()
{
   typedef typename std::conditional<
       std::numeric_limits<Number>::is_specialized,
       typename boost::multiprecision::number_category<Number>::type,
       std::integral_constant<int, 500> // not a number type
       >::type fp_test_type;

   test_specific<Number>(fp_test_type());

   //
   // Note really a test just yet, but we can at least print out all the values:
   //
   std::cout << "numeric_limits values for type " << typeid(Number).name() << std::endl;

   PRINT(is_specialized);
   BOOST_IF_CONSTEXPR(std::numeric_limits<Number>::is_integer)
   {
      std::cout << std::hex << std::showbase;
   }
   std::cout << "max()"
             << " = " << (std::numeric_limits<Number>::max)() << std::endl;
   BOOST_IF_CONSTEXPR(std::numeric_limits<Number>::is_integer)
   {
      std::cout << std::dec;
   }
   std::cout << "max()"
             << " = " << (std::numeric_limits<Number>::max)() << std::endl;
   std::cout << "min()"
             << " = " << (std::numeric_limits<Number>::min)() << std::endl;
   PRINT(lowest());
   PRINT(digits);
   PRINT(digits10);
   PRINT(max_digits10);
   PRINT(is_signed);
   PRINT(is_integer);
   PRINT(is_exact);
   PRINT(radix);
   PRINT(epsilon());
   PRINT(round_error());
   PRINT(min_exponent);
   PRINT(min_exponent10);
   PRINT(max_exponent);
   PRINT(max_exponent10);
   PRINT(has_infinity);
   PRINT(has_quiet_NaN);
   PRINT(has_signaling_NaN);
   PRINT(has_denorm);
   PRINT(has_denorm_loss);
   PRINT(infinity());
   PRINT(quiet_NaN());
   PRINT(signaling_NaN());
   PRINT(denorm_min());
   PRINT(is_iec559);
   PRINT(is_bounded);
   PRINT(is_modulo);
   PRINT(traps);
   PRINT(tinyness_before);
   PRINT(round_style);
}

#ifdef TEST_CPP_DOUBLE_FLOAT

#if !(defined(BOOST_MSVC) && (BOOST_MSVC <= 1900))
template <class Number>
void test_constexpr_ness()
{
   using local_float_type = Number;

   constexpr auto my_max = (std::numeric_limits<local_float_type>::max)();
   constexpr auto my_min = (std::numeric_limits<local_float_type>::min)();
   constexpr auto my_eps =  std::numeric_limits<local_float_type>::epsilon();
   constexpr auto my_nan =  std::numeric_limits<local_float_type>::quiet_NaN();
   constexpr auto my_inf =  std::numeric_limits<local_float_type>::infinity();

   static_assert(my_max > local_float_type(0), "Error: Can't handle max() in constexpr context");
   static_assert(my_min > local_float_type(0), "Error: Can't handle min() in constexpr context");
   static_assert(my_max > my_min,              "Error: Can't handle min()/max() in constexpr context");
   static_assert(my_eps > local_float_type(0), "Error: Can't handle epsilon() in constexpr context");
   static_assert(isnan(my_nan), "Error: Can't handle quiet_NaN() in constexpr context");
   static_assert(isinf(my_inf), "Error: Can't handle infinity() in constexpr context");

   static_assert((local_float_type(1) - my_eps) != local_float_type(1), "Error: Can't resolve epsilon() as the smallest number differing from one in constexpr context");
}

#endif

#endif

int main()
{
#ifdef TEST_BACKEND
   test<boost::multiprecision::number<boost::multiprecision::concepts::number_backend_float_architype> >();
#endif
#ifdef TEST_MPF_50
   test<boost::multiprecision::mpf_float_50>();
#endif
#ifdef TEST_MPF
   boost::multiprecision::mpf_float::default_precision(1000);
   /*
   boost::multiprecision::mpf_float r;
   r.precision(50);
   BOOST_TEST(r.precision() >= 50);
   */
   BOOST_TEST(boost::multiprecision::mpf_float::default_precision() == 1000);
   test<boost::multiprecision::mpf_float>();
#endif
#ifdef TEST_MPZ
   test<boost::multiprecision::mpz_int>();
#endif
#ifdef TEST_MPQ
   test<boost::multiprecision::mpq_rational>();
#endif
#ifdef TEST_CPP_DEC_FLOAT
   test<boost::multiprecision::cpp_dec_float_50>();
   test<boost::multiprecision::cpp_dec_float_100>();
#endif
#ifdef TEST_MPFR
   test<boost::multiprecision::mpfr_float>();
#endif
#ifdef TEST_MPFR_50
   test<boost::multiprecision::mpfr_float_50>();
#endif
#ifdef TEST_MPFI_50
   test<boost::multiprecision::mpfi_float_50>();
   test<boost::multiprecision::mpfi_float>();
#endif
#ifdef TEST_TOMMATH
   test<boost::multiprecision::tom_int>();
#endif
#ifdef TEST_CPP_INT
   test<boost::multiprecision::cpp_int>();
   test<boost::multiprecision::int256_t>();
   test<boost::multiprecision::uint512_t>();
   test<boost::multiprecision::number<boost::multiprecision::cpp_int_backend<200, 200, boost::multiprecision::unsigned_magnitude, boost::multiprecision::checked, void> > >();
   test<boost::multiprecision::number<boost::multiprecision::cpp_int_backend<70, 70, boost::multiprecision::signed_magnitude, boost::multiprecision::unchecked, void> > >();
#endif
#ifdef TEST_FLOAT128
   test<boost::multiprecision::float128>();
#endif
#ifdef TEST_CPP_BIN_FLOAT
#endif
#ifdef TEST_CPP_DOUBLE_FLOAT
   test<boost::multiprecision::cpp_double_float>();
   test<boost::multiprecision::cpp_double_double>();
   test<boost::multiprecision::cpp_double_long_double>();
   #if defined(BOOST_MP_CPP_DOUBLE_FP_HAS_FLOAT128)
   test<boost::multiprecision::cpp_double_float128>();
   #endif

   #if !(defined(BOOST_MSVC) && (BOOST_MSVC <= 1900))
   test_constexpr_ness<boost::multiprecision::cpp_double_float>();
   test_constexpr_ness<boost::multiprecision::cpp_double_double>();
   test_constexpr_ness<boost::multiprecision::cpp_double_long_double>();
   #endif
   #if defined(BOOST_MP_CPP_DOUBLE_FP_HAS_FLOAT128)
   test_constexpr_ness<boost::multiprecision::cpp_double_float128>();
   #endif
#endif
   return boost::report_errors();
}
