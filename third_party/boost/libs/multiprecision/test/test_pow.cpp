///////////////////////////////////////////////////////////////
//  Copyright 2011 - 2025 John Maddock.
//  Copyright Christopher Kormanyos 2021 - 2025.
//  Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt
//
// This work is based on an earlier work:
// "Algorithm 910: A Portable C++ Multiple-Precision System for Special-Function Calculations",
// in ACM TOMS, {VOL 37, ISSUE 4, (February 2011)} (C) ACM, 2011. http://doi.acm.org/10.1145/1916461.1916469

#ifdef _MSC_VER
#define _SCL_SECURE_NO_WARNINGS
#endif

#include <test.hpp>
#include <test_pow_data.hpp>
#include <test_pow_data_reduced.hpp>

#include <boost/detail/lightweight_test.hpp>
#include <boost/math/special_functions/fpclassify.hpp>

#if !defined(TEST_MPF_50) && !defined(TEST_MPF) && !defined(TEST_BACKEND) && !defined(TEST_CPP_DEC_FLOAT) && !defined(TEST_MPFR) && !defined(TEST_MPFR_50) && !defined(TEST_MPFI_50) && !defined(TEST_FLOAT128) && !defined(TEST_CPP_BIN_FLOAT) && !defined(TEST_CPP_DOUBLE_FLOAT)
#define TEST_MPF_50
//#  define TEST_MPF
#define TEST_BACKEND
#define TEST_CPP_DEC_FLOAT
#define TEST_FLOAT128
#define TEST_MPFR_50
#define TEST_MPFI_50
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

#if defined(TEST_MPF_50)
#include <boost/multiprecision/gmp.hpp>
#endif
#if defined(TEST_MPFR_50)
#include <boost/multiprecision/mpfr.hpp>
#endif
#if defined(TEST_MPFI_50)
#include <boost/multiprecision/mpfi.hpp>
#endif
#ifdef TEST_BACKEND
#include <boost/multiprecision/concepts/mp_number_archetypes.hpp>
#endif
#ifdef TEST_CPP_DEC_FLOAT
#include <boost/multiprecision/cpp_dec_float.hpp>
#endif
#ifdef TEST_FLOAT128
#include <boost/multiprecision/float128.hpp>
#endif
#ifdef TEST_CPP_BIN_FLOAT
#include <boost/multiprecision/cpp_bin_float.hpp>
#endif
#ifdef TEST_CPP_DOUBLE_FLOAT
#include <boost/multiprecision/cpp_double_fp.hpp>
#endif

#if (defined(TEST_CPP_DEC_FLOAT) || defined(TEST_CPP_BIN_FLOAT) || defined(TEST_CPP_DOUBLE_FLOAT) || defined(TEST_MPF_50) || defined(TEST_FLOAT128))
// These were the only ones I checked locally at the moment that use random tests.

#include <array>
#include <ctime>
#include <random>

#endif // (defined(TEST_CPP_DEC_FLOAT) || defined(TEST_CPP_BIN_FLOAT) || defined(TEST_CPP_DOUBLE_FLOAT) || defined(TEST_MPF_50) || defined(TEST_FLOAT128))

template<typename FloatType> auto my_zero() -> FloatType&;

namespace local {

   template <class DataArrayType>
   struct data_maker
   {
   private:
      using local_array_type = DataArrayType;

   public:
      static auto get_data() -> const local_array_type&
      {
         // We do not expect to ever instantiate this struct.

         static const local_array_type instance { };

         return instance;
      }
   };

   template <>
   struct data_maker<test_pow_data::test_pow_data_array_type_reduced> final
   {
   private:
      using local_array_type = test_pow_data::test_pow_data_array_type_reduced;

   public:
      static auto get_data() -> const local_array_type&
      {
         static const local_array_type instance { test_pow_data::data_reduced };

         return instance;
      }
   };

   template <>
   struct data_maker<test_pow_data::test_pow_data_array_type_default> final
   {
   private:
      using local_array_type = test_pow_data::test_pow_data_array_type_default;

   public:
      static auto get_data() -> const local_array_type&
      {
         static const local_array_type instance { test_pow_data::data };

         return instance;
      }
   };

} // namespace local

#if (defined(TEST_CPP_DEC_FLOAT) || defined(TEST_CPP_BIN_FLOAT) || defined(TEST_CPP_DOUBLE_FLOAT) || defined(TEST_MPF_50) || defined(TEST_FLOAT128))

// These were the only ones I checked locally at the moment.

// TEST_MPF_50 is included (even though it won't be tested)
// because its value of has_quiet_NaN is false. This has been
// done intentionally. Its inclusion here is intended to verify
// the constexpr logic implemented in test_issue722().

template <class T>
void test_issue722()
{
   BOOST_IF_CONSTEXPR (std::numeric_limits<T>::is_specialized && std::numeric_limits<T>::has_infinity && std::numeric_limits<T>::has_quiet_NaN)
   {
      std::mt19937_64 gen { };

      gen.seed(static_cast<typename std::mt19937_64::result_type>(std::clock()));

      std::uniform_real_distribution<float>
         dist
         (
           static_cast<float>(1.01L),
           static_cast<float>(1.04L)
         );

      for (int index = 0; index < 8; ++index)
      {
         static_cast<void>(index);

         const T neg_inf = T(-std::numeric_limits<T>::infinity() * dist(gen));

         const T pow_neg_inf_to_the_nan = pow(neg_inf, std::numeric_limits<T>::quiet_NaN() * dist(gen));

         BOOST_CHECK((boost::multiprecision::isnan)(pow_neg_inf_to_the_nan));
      }

      for (int index = 0; index < 8; ++index)
      {
         static_cast<void>(index);

         const T val_x_zero { ::my_zero<T>() * dist(gen) };
         const T val_a_zero { ::my_zero<T>() * dist(gen) };

         const T pow_zero_to_the_zero = pow(val_x_zero, val_a_zero);

         BOOST_CHECK(pow_zero_to_the_zero == 1);
      }
   }
}

template <class T>
void test_small_a_in_default_ops()
{
   // Table[N[(789/1000)^(10^(n)), 101], {n, -12, -6, 1}]

   using small_a_data_array_type = std::array<T, std::size_t { UINT8_C(7) }>;

   const small_a_data_array_type small_a_data =
   {{
      T("0.99999999999976301104186376527231033639051940385775656688844414344774455623761946001430627233999497696"),
      T("0.99999999999763011041864018009258589471870605197334212386735372254907219885974849025436678031854699987"),
      T("0.99999999997630110418665453787411005197468225239383193650701563365941781771201272808876889833419359752"),
      T("0.99999999976301104189181907356423443492950586157450272561606631208248254028299217451384055629333440182"),
      T("0.99999999763011042144556021597917228924335227306284422554015278215963516103516330474932028841209422025"),
      T("0.99999997630110446719254821691095026610571257635164394962451615449422537744479656992450152929555193362"),
      T("0.99999976301106994561811131758260769485991144451216442896857591003045319114657385381607667593523204524")
   }};

   unsigned max_err = 0U;

   const T x_val(T(789U) / 1000);

   const T local_tol { std::numeric_limits<T>::epsilon() / 8 };

   T small_a = pow(T(10), -12);

   for (std::size_t k { UINT8_C(0) }; k < std::tuple_size<small_a_data_array_type>::value; ++k)
   {
      const T val = pow(x_val, small_a);

      small_a *= 10;

      using std::fabs;

      const T delta { val / small_a_data[k] };
      const T closeness { fabs(1 - delta) };

      const T rel = closeness / local_tol;

      unsigned err = rel.template convert_to<unsigned>();

      if (err > max_err)
      {
         max_err = err;
      }
   }

   std::cout << "Max error data having small a: " << max_err << std::endl;

   BOOST_TEST(max_err < 64U);
}

#endif // (defined(TEST_CPP_DEC_FLOAT) || defined(TEST_CPP_BIN_FLOAT) || defined(TEST_CPP_DOUBLE_FLOAT) || defined(TEST_MPF_50) || defined(TEST_FLOAT128))

template <class T>
void test()
{
   std::cout << "Testing type: " << typeid(T).name() << std::endl;

   using local_test_pow_data_array_type =
      typename std::conditional<::has_poor_exp_range_or_precision_support<T>::value,
                                test_pow_data::test_pow_data_array_type_reduced,
                                test_pow_data::test_pow_data_array_type_default>::type;

   const auto& data = local::data_maker<local_test_pow_data_array_type>::get_data();

   unsigned max_err = 0U;
   for (unsigned k = 0; k < data.size(); k++)
   {
      T        val = pow(T(data[k][0]), T(data[k][1]));
      T        e   = relative_error(val, T(data[k][2]));
      unsigned err = e.template convert_to<unsigned>();
      if (err > max_err)
      {
         max_err = err;
      }
      val = pow(T(data[k][0]), -T(data[k][1]));
      e   = relative_error(val, T(1 / T(data[k][2])));
      err = e.template convert_to<unsigned>();
      if (err > max_err)
      {
         max_err = err;
      }
   }

   std::cout << "Max error from data table was: " << max_err << std::endl;
#if defined(BOOST_INTEL) && defined(TEST_FLOAT128)
   BOOST_TEST(max_err < 40000U);
#else
   BOOST_TEST(max_err < 8000U);
#endif

   //
   // Special cases and edge-cases come last:
   //
   BOOST_CHECK_EQUAL(pow(T(0), T(0)), 1);
   BOOST_CHECK_EQUAL(pow(T(0), 0), 1);
   BOOST_CHECK_EQUAL(pow(T(0), T(2)), 0);
   BOOST_CHECK_EQUAL(pow(T(0), 2), 0);
   BOOST_CHECK_EQUAL(pow(T(1), T(0)), 1);
   BOOST_CHECK_EQUAL(pow(T(1), 0), 1);
   BOOST_CHECK_EQUAL(pow(T(1), T(2)), 1);
   BOOST_CHECK_EQUAL(pow(T(1), 2), 1);

   #if (defined(TEST_CPP_DEC_FLOAT) || defined(TEST_CPP_BIN_FLOAT) || defined(TEST_CPP_DOUBLE_FLOAT) || defined(TEST_MPF_50) || defined(TEST_FLOAT128))
   // These were the only ones I checked locally at the moment.

   std::mt19937_64 gen { };

   gen.seed(static_cast<typename std::mt19937_64::result_type>(std::clock()));

   std::uniform_real_distribution<float>
      dist
      (
        static_cast<float>(1.01L),
        static_cast<float>(1.04L)
      );

   #endif // (defined(TEST_CPP_DEC_FLOAT) || defined(TEST_CPP_BIN_FLOAT) || defined(TEST_CPP_DOUBLE_FLOAT) || defined(TEST_MPF_50) || defined(TEST_FLOAT128))

   BOOST_IF_CONSTEXPR(std::numeric_limits<T>::is_specialized && std::numeric_limits<T>::has_infinity)
   {
      BOOST_CHECK_EQUAL(pow(T(1.25F), std::numeric_limits<T>::infinity()), std::numeric_limits<T>::infinity());
      BOOST_CHECK_EQUAL(pow(T(1.25F), -std::numeric_limits<T>::infinity()), T(0));


      #if (defined(TEST_CPP_DEC_FLOAT) || defined(TEST_CPP_BIN_FLOAT) || defined(TEST_CPP_DOUBLE_FLOAT) || defined(TEST_MPF_50) || defined(TEST_FLOAT128))
      // These were the only ones I checked locally at the moment.

      for (int npow = -8; npow < 8; ++npow)
      {
         T zero_arg = my_zero<T>() * dist(gen);

         const T pow_zero_to_a = pow(zero_arg, T(static_cast<float>(npow) * dist(gen)));

         if (npow < 0)
         {
            BOOST_CHECK((boost::multiprecision::isinf)(pow_zero_to_a));
         }
         else if (npow > 0)
         {
            BOOST_CHECK_EQUAL(pow_zero_to_a, T(0));
         }
         else
         {
            BOOST_CHECK_EQUAL(pow_zero_to_a, T(1));
         }
      }

      for (int narg = 2; narg < 8; ++narg)
      {
         T finite_arg = T(static_cast<float>(narg) * dist(gen));

         const T pow_finite_to_zero = pow(finite_arg, my_zero<T>() * dist(gen));

         BOOST_CHECK_EQUAL(pow_finite_to_zero, T(1));
      }

      for (int nexp = -8; nexp < 8; ++nexp)
      {
         T finite_exp = T(static_cast<float>(nexp) * dist(gen));

         const T pow_infinite_to_finite = pow(std::numeric_limits<T>::infinity() * dist(gen), finite_exp);

         if (nexp < 0)
         {
            BOOST_CHECK_EQUAL(pow_infinite_to_finite, T(0));
         }
         else if (nexp > 0)
         {
            BOOST_CHECK((boost::multiprecision::isinf)(pow_infinite_to_finite));
         }
         else
         {
            BOOST_CHECK_EQUAL(pow_infinite_to_finite, T(1));
         }
      }

      BOOST_IF_CONSTEXPR(std::numeric_limits<T>::is_specialized && std::numeric_limits<T>::has_quiet_NaN)
      {
         for (int index = 0; index < 8; ++index)
         {
            static_cast<void>(index);

            T inf_or_zero_arg = T(::my_zero<T>() * dist(gen));

            T pow_inf_or_zero_to_the_nan = pow(inf_or_zero_arg, std::numeric_limits<T>::quiet_NaN() * dist(gen));

            BOOST_CHECK((boost::multiprecision::isnan)(pow_inf_or_zero_to_the_nan));

            inf_or_zero_arg = T(std::numeric_limits<T>::infinity() * dist(gen));

            pow_inf_or_zero_to_the_nan = pow(inf_or_zero_arg, std::numeric_limits<T>::quiet_NaN() * dist(gen));

            BOOST_CHECK((boost::multiprecision::isnan)(pow_inf_or_zero_to_the_nan));
         }
      }

      #endif // (defined(TEST_CPP_DEC_FLOAT) || defined(TEST_CPP_BIN_FLOAT) || defined(TEST_CPP_DOUBLE_FLOAT) || defined(TEST_MPF_50) || defined(TEST_FLOAT128))
   }

   BOOST_IF_CONSTEXPR((!boost::multiprecision::is_interval_number<T>::value) && std::numeric_limits<T>::is_specialized && std::numeric_limits<T>::has_quiet_NaN)
   {
      BOOST_CHECK((boost::math::isnan)(pow(T(1.25F), std::numeric_limits<T>::quiet_NaN())));

      BOOST_CHECK_EQUAL(pow(std::numeric_limits<T>::quiet_NaN(), T(0)), T(1));

      BOOST_CHECK((boost::math::isnan)(pow(std::numeric_limits<T>::quiet_NaN(), T(1.25F))));
   }

   if (!boost::multiprecision::is_interval_number<T>::value)
   {
      T bug_case = -1.05 * log((std::numeric_limits<T>::max)()) / log(T(1.01));

      for (unsigned i = 0; i < 100; ++i, bug_case *= 1.05)
      {
         if (std::numeric_limits<T>::has_infinity)
         {
            BOOST_CHECK_EQUAL(pow(T(1.01), bug_case), 0);
         }
         else
         {
            BOOST_CHECK_LE(pow(T(1.01), bug_case), (std::numeric_limits<T>::min)());
         }
      }
   }
}

int main()
{
#ifdef TEST_BACKEND
   test<boost::multiprecision::number<boost::multiprecision::concepts::number_backend_float_architype> >();
#endif
#ifdef TEST_MPF_50
   test<boost::multiprecision::mpf_float_50>();
   test<boost::multiprecision::mpf_float_100>();
   test_issue722<boost::multiprecision::mpf_float_50>();
   test_small_a_in_default_ops<boost::multiprecision::mpf_float_50>();
#endif
#ifdef TEST_MPFR_50
   test<boost::multiprecision::mpfr_float_50>();
   test<boost::multiprecision::mpfr_float_100>();
#endif
#ifdef TEST_MPFI_50
   test<boost::multiprecision::mpfi_float_50>();
   test<boost::multiprecision::mpfi_float_100>();
#endif
#ifdef TEST_CPP_DEC_FLOAT
   test<boost::multiprecision::cpp_dec_float_50>();
   test<boost::multiprecision::cpp_dec_float_100>();
   test_issue722<boost::multiprecision::cpp_dec_float_50>();
   test_small_a_in_default_ops<boost::multiprecision::cpp_dec_float_50>();
#ifndef SLOW_COMPLER
   // Some "peculiar" digit counts which stress our code:
   test<boost::multiprecision::number<boost::multiprecision::cpp_dec_float<65> > >();
   test<boost::multiprecision::number<boost::multiprecision::cpp_dec_float<64> > >();
   test<boost::multiprecision::number<boost::multiprecision::cpp_dec_float<63> > >();
   test<boost::multiprecision::number<boost::multiprecision::cpp_dec_float<62> > >();
   test<boost::multiprecision::number<boost::multiprecision::cpp_dec_float<61, long long> > >();
   test<boost::multiprecision::number<boost::multiprecision::cpp_dec_float<60, long long> > >();
   test<boost::multiprecision::number<boost::multiprecision::cpp_dec_float<59, long long, std::allocator<char> > > >();
   test<boost::multiprecision::number<boost::multiprecision::cpp_dec_float<58, long long, std::allocator<char> > > >();
   // Check low multiprecision digit counts.
   test<boost::multiprecision::number<boost::multiprecision::cpp_dec_float<9> > >();
   test<boost::multiprecision::number<boost::multiprecision::cpp_dec_float<18> > >();
#endif
#endif
#ifdef TEST_FLOAT128
   test<boost::multiprecision::float128>();
   test_issue722<boost::multiprecision::float128>();
   test_small_a_in_default_ops<boost::multiprecision::float128>();
#endif
#ifdef TEST_CPP_BIN_FLOAT
   test<boost::multiprecision::cpp_bin_float_50>();
   test<boost::multiprecision::number<boost::multiprecision::cpp_bin_float<35, boost::multiprecision::digit_base_10, std::allocator<char>, long long> > >();
   test_issue722<boost::multiprecision::cpp_bin_float_50>();
   test_small_a_in_default_ops<boost::multiprecision::cpp_bin_float_50>();
#endif
#ifdef TEST_CPP_DOUBLE_FLOAT
   test<boost::multiprecision::cpp_double_double>();
   test<boost::multiprecision::cpp_double_long_double>();
   #if defined(BOOST_MP_CPP_DOUBLE_FP_HAS_FLOAT128)
   test<boost::multiprecision::cpp_double_float128>();
   #endif
   test_issue722<boost::multiprecision::cpp_double_double>();
   test_small_a_in_default_ops<boost::multiprecision::cpp_double_double>();
   test_small_a_in_default_ops<boost::multiprecision::cpp_double_long_double>();
   #if defined(BOOST_MP_CPP_DOUBLE_FP_HAS_FLOAT128)
   test_small_a_in_default_ops<boost::multiprecision::cpp_double_float128>();
   #endif
#endif
   return boost::report_errors();
}

template<typename FloatType> auto my_zero() -> FloatType&
{
   using float_type = FloatType;

   static float_type my_val_zero(0);

   return my_val_zero;
}
