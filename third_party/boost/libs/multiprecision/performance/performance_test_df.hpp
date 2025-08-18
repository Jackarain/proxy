///////////////////////////////////////////////////////////////
//  Copyright John Maddock 2019.
//  Copyright Christopher Kormanyos 2021 - 2025.
//  Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt

// This version of "performance_test_df.hpp" is based almost entirely on
// the original "performance_test.cpp". But it has been specifically
// adapted to test various backends in small precision ranges in order
// to check the performance comparison with cpp_double_double.

#ifndef PERFORMANCE_TEST_DF_2025_01_09_HPP
#define PERFORMANCE_TEST_DF_2025_01_09_HPP

#define BOOST_CHRONO_HEADER_ONLY

#ifdef _MSC_VER
#define _SCL_SECURE_NO_WARNINGS
#endif

#if !defined(TEST_CPP_DEC_FLOAT) && !defined(TEST_CPP_BIN_FLOAT) && !defined(TEST_FLOAT128) && !defined(TEST_CPP_DOUBLE_FLOAT)
#define TEST_CPP_DEC_FLOAT
#define TEST_CPP_BIN_FLOAT
#define TEST_CPP_DOUBLE_FLOAT
#define TEST_FLOAT128

#ifdef _MSC_VER
#pragma message("CAUTION!!: No backend type specified so testing everything.... this will take some time!!")
#endif
#ifdef __GNUC__
#pragma warning "CAUTION!!: No backend type specified so testing everything.... this will take some time!!"
#endif

#endif

#include <boost/chrono.hpp>
#include <vector>
#include <map>
#include <string>
#include <cstring>
#include <cctype>
#include <iostream>
#include <iomanip>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/multiprecision/number.hpp>

template <class Clock>
struct stopwatch
{
   typedef typename Clock::duration duration;
   stopwatch()
   {
      m_start = Clock::now();
   }
   duration elapsed()
   {
      return Clock::now() - m_start;
   }
   void reset()
   {
      m_start = Clock::now();
   }

 private:
   typename Clock::time_point m_start;
};

extern unsigned bits_wanted; // for integer types

template <class T, int Type>
struct tester
{
   static constexpr unsigned rounds()
   {
      return
         static_cast<unsigned>
         (
            (Type == boost::multiprecision::number_kind_floating_point)
               ? (std::numeric_limits<T>::is_specialized && (std::numeric_limits<T>::digits10 <  51)) ? 8000 :
                 (std::numeric_limits<T>::is_specialized && (std::numeric_limits<T>::digits10 < 101)) ? 4000 : 1000
               : 1000
         );
   }

   tester()
   {
      a.assign(500, 0);
      for (int i = 0; i < 500; ++i)
      {
         b.push_back(generate_random());
         c.push_back(generate_random());
         small.push_back(gen());
      }
   }
   double test_add()
   {
      stopwatch<boost::chrono::high_resolution_clock> w;
      for (unsigned i = 0; i < rounds(); ++i)
      {
         for (unsigned j = 0; j < b.size(); ++j)
            a[j] = b[j] + c[j];
      }
      return boost::chrono::duration_cast<boost::chrono::duration<double> >(w.elapsed()).count();
   }
   double test_subtract()
   {
      stopwatch<boost::chrono::high_resolution_clock> w;
      for (unsigned i = 0; i < rounds(); ++i)
      {
         for (unsigned j = 0; j < b.size(); ++j)
            a[j] = b[j] - c[j];
      }
      return boost::chrono::duration_cast<boost::chrono::duration<double> >(w.elapsed()).count();
   }
   double test_add_int()
   {
      stopwatch<boost::chrono::high_resolution_clock> w;
      for (unsigned i = 0; i < rounds(); ++i)
      {
         for (unsigned j = 0; j < b.size(); ++j)
            a[j] = b[j] + 1;
      }
      return boost::chrono::duration_cast<boost::chrono::duration<double> >(w.elapsed()).count();
   }
   double test_subtract_int()
   {
      stopwatch<boost::chrono::high_resolution_clock> w;
      for (unsigned i = 0; i < rounds(); ++i)
      {
         for (unsigned j = 0; j < b.size(); ++j)
            a[j] = b[j] - 1;
      }
      return boost::chrono::duration_cast<boost::chrono::duration<double> >(w.elapsed()).count();
   }
   double test_multiply()
   {
      stopwatch<boost::chrono::high_resolution_clock> w;
      for (unsigned i = 0; i < rounds(); ++i)
      {
         for (unsigned k = 0; k < b.size(); ++k)
            a[k] = b[k] * c[k];
      }
      return boost::chrono::duration_cast<boost::chrono::duration<double> >(w.elapsed()).count();
   }
   double test_multiply_int()
   {
      stopwatch<boost::chrono::high_resolution_clock> w;
      for (unsigned i = 0; i < rounds(); ++i)
      {
         for (unsigned j = 0; j < b.size(); ++j)
            a[j] = b[j] * 3;
      }
      return boost::chrono::duration_cast<boost::chrono::duration<double> >(w.elapsed()).count();
   }
   double test_divide()
   {
      stopwatch<boost::chrono::high_resolution_clock> w;
      for (unsigned i = 0; i < rounds(); ++i)
      {
         for (unsigned j = 0; j < b.size(); ++j)
            a[j] = b[j] / c[j] + b[j] / small[j];
      }
      return boost::chrono::duration_cast<boost::chrono::duration<double> >(w.elapsed()).count();
   }
   double test_divide_int()
   {
      stopwatch<boost::chrono::high_resolution_clock> w;
      for (unsigned i = 0; i < rounds(); ++i)
      {
         for (unsigned j = 0; j < b.size(); ++j)
            a[j] = b[j] / 3;
      }
      return boost::chrono::duration_cast<boost::chrono::duration<double> >(w.elapsed()).count();
   }
   double test_sqrt()
   {
      stopwatch<boost::chrono::high_resolution_clock> w;
      for (unsigned i = 0; i < rounds(); ++i)
      {
         for (unsigned j = 0; j < b.size(); ++j)
            a[j] = sqrt(b[j]);
      }
      return boost::chrono::duration_cast<boost::chrono::duration<double> >(w.elapsed()).count();
   }
   double test_log()
   {
      stopwatch<boost::chrono::high_resolution_clock> w;
      for (unsigned i = 0; i < rounds(); ++i)
      {
         for (unsigned j = 0; j < b.size(); ++j)
            a[j] = log(b[j]);
      }
      return boost::chrono::duration_cast<boost::chrono::duration<double> >(w.elapsed()).count();
   }
   double test_exp()
   {
      stopwatch<boost::chrono::high_resolution_clock> w;
      for (unsigned i = 0; i < rounds(); ++i)
      {
         for (unsigned j = 0; j < b.size(); ++j)
            a[j] = exp(b[j]);
      }
      return boost::chrono::duration_cast<boost::chrono::duration<double> >(w.elapsed()).count();
   }
   double test_tan()
   {
      stopwatch<boost::chrono::high_resolution_clock> w;
      for (unsigned i = 0; i < rounds(); ++i)
      {
         for (unsigned j = 0; j < b.size(); ++j)
            a[j] = exp(b[j]);
      }
      return boost::chrono::duration_cast<boost::chrono::duration<double> >(w.elapsed()).count();
   }
   double test_str(const std::integral_constant<bool, false>&)
   {
      stopwatch<boost::chrono::high_resolution_clock> w;
      for (unsigned i = 0; i < b.size(); ++i)
         a[i] = boost::lexical_cast<T>(boost::lexical_cast<std::string>(b[i]));
      return boost::chrono::duration_cast<boost::chrono::duration<double> >(w.elapsed()).count();
   }
   double test_str(const std::integral_constant<bool, true>&)
   {
      stopwatch<boost::chrono::high_resolution_clock> w;
      for (unsigned i = 0; i < b.size(); ++i)
         a[i].assign(b[i].str());
      return boost::chrono::duration_cast<boost::chrono::duration<double> >(w.elapsed()).count();
   }
   double test_str()
   {
      return test_str(std::is_class<T>());
   }
   //
   // The following tests only work for integer types:
   //
   double test_mod()
   {
      stopwatch<boost::chrono::high_resolution_clock> w;
      for (unsigned i = 0; i < rounds(); ++i)
      {
         for (unsigned i = 0; i < b.size(); ++i)
            a[i] = b[i] % c[i] + b[i] % small[i];
      }
      return boost::chrono::duration_cast<boost::chrono::duration<double> >(w.elapsed()).count();
   }
   double test_mod_int()
   {
      stopwatch<boost::chrono::high_resolution_clock> w;
      for (unsigned i = 0; i < rounds(); ++i)
      {
         for (unsigned i = 0; i < b.size(); ++i)
            a[i] = b[i] % 254;
      }
      return boost::chrono::duration_cast<boost::chrono::duration<double> >(w.elapsed()).count();
   }
   double test_or()
   {
      stopwatch<boost::chrono::high_resolution_clock> w;
      for (unsigned i = 0; i < rounds(); ++i)
      {
         for (unsigned i = 0; i < b.size(); ++i)
            a[i] = b[i] | c[i];
      }
      return boost::chrono::duration_cast<boost::chrono::duration<double> >(w.elapsed()).count();
   }
   double test_or_int()
   {
      stopwatch<boost::chrono::high_resolution_clock> w;
      for (unsigned i = 0; i < rounds(); ++i)
      {
         for (unsigned i = 0; i < b.size(); ++i)
            a[i] = b[i] | 234;
      }
      return boost::chrono::duration_cast<boost::chrono::duration<double> >(w.elapsed()).count();
   }
   double test_and()
   {
      stopwatch<boost::chrono::high_resolution_clock> w;
      for (unsigned i = 0; i < rounds(); ++i)
      {
         for (unsigned i = 0; i < b.size(); ++i)
            a[i] = b[i] & c[i];
      }
      return boost::chrono::duration_cast<boost::chrono::duration<double> >(w.elapsed()).count();
   }
   double test_and_int()
   {
      stopwatch<boost::chrono::high_resolution_clock> w;
      for (unsigned i = 0; i < rounds(); ++i)
      {
         for (unsigned i = 0; i < b.size(); ++i)
            a[i] = b[i] & 234;
      }
      return boost::chrono::duration_cast<boost::chrono::duration<double> >(w.elapsed()).count();
   }
   double test_xor()
   {
      stopwatch<boost::chrono::high_resolution_clock> w;
      for (unsigned i = 0; i < rounds(); ++i)
      {
         for (unsigned i = 0; i < b.size(); ++i)
            a[i] = b[i] ^ c[i];
      }
      return boost::chrono::duration_cast<boost::chrono::duration<double> >(w.elapsed()).count();
   }
   double test_xor_int()
   {
      stopwatch<boost::chrono::high_resolution_clock> w;
      for (unsigned i = 0; i < rounds(); ++i)
      {
         for (unsigned i = 0; i < b.size(); ++i)
            a[i] = b[i] ^ 234;
      }
      return boost::chrono::duration_cast<boost::chrono::duration<double> >(w.elapsed()).count();
   }
   double test_complement()
   {
      stopwatch<boost::chrono::high_resolution_clock> w;
      for (unsigned i = 0; i < rounds(); ++i)
      {
         for (unsigned i = 0; i < b.size(); ++i)
            a[i] = ~b[i];
      }
      return boost::chrono::duration_cast<boost::chrono::duration<double> >(w.elapsed()).count();
   }
   double test_left_shift()
   {
      int                                             max_shift = std::numeric_limits<T>::is_bounded ? std::numeric_limits<T>::digits : bits_wanted;
      int                                             shift     = 0;
      stopwatch<boost::chrono::high_resolution_clock> w;
      for (unsigned i = 0; i < rounds(); ++i)
      {
         for (unsigned i = 0; i < b.size(); ++i)
            a[i] = b[i] << (shift++ % max_shift);
      }
      return boost::chrono::duration_cast<boost::chrono::duration<double> >(w.elapsed()).count();
   }
   double test_right_shift()
   {
      int                                             max_shift = 2 + std::numeric_limits<T>::is_bounded ? std::numeric_limits<T>::digits : bits_wanted;
      int                                             shift     = 0;
      stopwatch<boost::chrono::high_resolution_clock> w;
      for (unsigned i = 0; i < rounds(); ++i)
      {
         for (unsigned i = 0; i < b.size(); ++i)
            a[i] = b[i] >> (shift++) % max_shift;
      }
      return boost::chrono::duration_cast<boost::chrono::duration<double> >(w.elapsed()).count();
   }
   double test_gcd()
   {
      using boost::integer::gcd;
      stopwatch<boost::chrono::high_resolution_clock> w;
      for (unsigned i = 0; i < rounds(); ++i)
      {
         for (unsigned i = 0; i < b.size(); ++i)
            a[i] = gcd(b[i], c[i]);
      }
      return boost::chrono::duration_cast<boost::chrono::duration<double> >(w.elapsed()).count();
   }
   double test_powm()
   {
      stopwatch<boost::chrono::high_resolution_clock> w;
      for (unsigned i = 0; i < 25; ++i)
      {
         for (unsigned i = 0; i < b.size(); ++i)
            a[i] = powm(b[i], b[i] / 2, c[i]);
      }
      return boost::chrono::duration_cast<boost::chrono::duration<double> >(w.elapsed()).count();
   }
   double test_construct()
   {
      std::allocator<T>                               alloc;
      T*                                              pt = alloc.allocate(1000);
      stopwatch<boost::chrono::high_resolution_clock> w;
      for (unsigned i = 0; i < rounds(); ++i)
      {
         for (unsigned j = 0; j < 1000; ++j)
            new (pt + j) T();
         for (unsigned j = 0; j < 1000; ++j)
            std::allocator_traits<std::allocator<T> >::destroy(alloc, pt + j);
      }
      double result = boost::chrono::duration_cast<boost::chrono::duration<double> >(w.elapsed()).count();
      alloc.deallocate(pt, 1000);
      return result;
   }
   double test_construct_unsigned()
   {
      std::allocator<T>                               alloc;
      T*                                              pt = alloc.allocate(1000);
      stopwatch<boost::chrono::high_resolution_clock> w;
      for (unsigned i = 0; i < rounds(); ++i)
      {
         for (unsigned j = 0; j < 1000; ++j)
            new (pt + j) T(j);
         for (unsigned j = 0; j < 1000; ++j)
            std::allocator_traits<std::allocator<T> >::destroy(alloc, pt + j);
      }
      double result = boost::chrono::duration_cast<boost::chrono::duration<double> >(w.elapsed()).count();
      alloc.deallocate(pt, 1000);
      return result;
   }
   double test_construct_unsigned_ll()
   {
      std::allocator<T>                               alloc;
      T*                                              pt = alloc.allocate(1000);
      stopwatch<boost::chrono::high_resolution_clock> w;
      for (unsigned i = 0; i < rounds(); ++i)
      {
         for (unsigned long long j = 0; j < 1000; ++j)
            new (pt + j) T(j);
         for (unsigned j = 0; j < 1000; ++j)
            std::allocator_traits<std::allocator<T>  >::destroy(alloc, pt + j);
      }
      double result = boost::chrono::duration_cast<boost::chrono::duration<double> >(w.elapsed()).count();
      alloc.deallocate(pt, 1000);
      return result;
   }

   //
   // Hetero operations:
   //
   template <class U>
   static U get_hetero_test_value(std::integral_constant<bool, false> const&)
   {
      return U(2) / 3;
   }
   template <class U>
   static U get_hetero_test_value(std::integral_constant<bool, true> const&)
   {
      return (std::numeric_limits<U>::max)() >> 4;
   }
   template <class U>
   static U get_hetero_test_value()
   {
      return get_hetero_test_value<U>(boost::multiprecision::detail::is_integral<U>());
   }
   template <class U>
   double test_multiply_hetero()
   {
      const std::vector<U>&                           vals = get_hetero_test_vector<U>();
      stopwatch<boost::chrono::high_resolution_clock> w;
      for (unsigned i = 0; i < rounds(); ++i)
      {
         for (unsigned j = 0; j < b.size(); ++j)
            a[j] = b[j] * vals[j];
      }
      return boost::chrono::duration_cast<boost::chrono::duration<double> >(w.elapsed()).count();
   }
   template <class U>
   double test_inplace_multiply_hetero()
   {
      const std::vector<U>&                           vals = get_hetero_test_vector<U>();
      stopwatch<boost::chrono::high_resolution_clock> w;
      for (unsigned i = 0; i < rounds(); ++i)
      {
         for (unsigned j = 0; j < b.size(); ++j)
            a[j] = b[j], a[j] *= vals[j];
      }
      return boost::chrono::duration_cast<boost::chrono::duration<double> >(w.elapsed()).count();
   }
   template <class U>
   double test_add_hetero()
   {
      const std::vector<U>&                           vals = get_hetero_test_vector<U>();
      stopwatch<boost::chrono::high_resolution_clock> w;
      for (unsigned i = 0; i < rounds(); ++i)
      {
         for (unsigned j = 0; j < b.size(); ++j)
            a[j] = b[j] + vals[j];
      }
      return boost::chrono::duration_cast<boost::chrono::duration<double> >(w.elapsed()).count();
   }
   template <class U>
   double test_inplace_add_hetero()
   {
      const std::vector<U>&                           vals = get_hetero_test_vector<U>();
      stopwatch<boost::chrono::high_resolution_clock> w;
      for (unsigned i = 0; i < rounds(); ++i)
      {
         for (unsigned j = 0; j < b.size(); ++j)
            a[j] = b[j], a[j] += vals[j];
      }
      return boost::chrono::duration_cast<boost::chrono::duration<double> >(w.elapsed()).count();
   }
   template <class U>
   double test_subtract_hetero()
   {
      const std::vector<U>&                           vals = get_hetero_test_vector<U>();
      stopwatch<boost::chrono::high_resolution_clock> w;
      for (unsigned i = 0; i < rounds(); ++i)
      {
         for (unsigned j = 0; j < b.size(); ++j)
            a[j] = b[j] - vals[j];
      }
      return boost::chrono::duration_cast<boost::chrono::duration<double> >(w.elapsed()).count();
   }
   template <class U>
   double test_inplace_subtract_hetero()
   {
      const std::vector<U>&                           vals = get_hetero_test_vector<U>();
      stopwatch<boost::chrono::high_resolution_clock> w;
      for (unsigned i = 0; i < rounds(); ++i)
      {
         for (unsigned j = 0; j < b.size(); ++j)
            a[j] = b[j], a[j] -= vals[j];
      }
      return boost::chrono::duration_cast<boost::chrono::duration<double> >(w.elapsed()).count();
   }
   template <class U>
   double test_divide_hetero()
   {
      const std::vector<U>&                           vals = get_hetero_test_vector<U>();
      stopwatch<boost::chrono::high_resolution_clock> w;
      for (unsigned i = 0; i < rounds(); ++i)
      {
         for (unsigned j = 0; j < b.size(); ++j)
            a[j] = b[j] / vals[j];
      }
      return boost::chrono::duration_cast<boost::chrono::duration<double> >(w.elapsed()).count();
   }
   template <class U>
   double test_inplace_divide_hetero()
   {
      const std::vector<U>&                           vals = get_hetero_test_vector<U>();
      stopwatch<boost::chrono::high_resolution_clock> w;
      for (unsigned i = 0; i < rounds(); ++i)
      {
         for (unsigned j = 0; j < b.size(); ++j)
            a[j] = b[j], a[j] /= vals[j];
      }
      return boost::chrono::duration_cast<boost::chrono::duration<double> >(w.elapsed()).count();
   }

 private:
   T generate_random()
   {
      return generate_random<T>(std::integral_constant<int, Type>());
   }
   template <class U>
   U generate_random(const std::integral_constant<int, boost::multiprecision::number_kind_floating_point>&)
   {
      U val      = gen();
      U prev_val = -1;
      while (val != prev_val)
      {
         val *= (gen.max)();
         prev_val = val;
         val += gen();
      }
      int e;
      val = frexp(val, &e);

      typedef typename U::backend_type::exponent_type        e_type;
      static boost::random::uniform_int_distribution<e_type> ui(-30, 30);
      return ldexp(val, static_cast<int>(ui(gen)));
   }
   template <class U>
   U generate_random(const std::integral_constant<int, boost::multiprecision::number_kind_integer>&)
   {
      typedef boost::random::mt19937::result_type random_type;

      U        max_val;
      unsigned digits;
      if (std::numeric_limits<U>::is_bounded)
      {
         max_val = (std::numeric_limits<U>::max)();
         digits  = std::numeric_limits<U>::digits;
      }
      else
      {
         max_val = U(1) << bits_wanted;
         digits  = bits_wanted;
      }

      unsigned bits_per_r_val = std::numeric_limits<random_type>::digits - 1;
      while ((random_type(1) << bits_per_r_val) > (gen.max)())
         --bits_per_r_val;

      unsigned terms_needed = digits / bits_per_r_val + 1;

      U val = 0;
      for (unsigned i = 0; i < terms_needed; ++i)
      {
         val *= (gen.max)();
         val += gen();
      }
      val %= max_val;
      return val;
   }
   template <class U>
   U generate_random(const std::integral_constant<int, boost::multiprecision::number_kind_rational>&)
   {
      typedef boost::random::mt19937::result_type                     random_type;
      typedef typename boost::multiprecision::component_type<U>::type IntType;

      IntType  max_val;
      unsigned digits;
      if (std::numeric_limits<IntType>::is_bounded)
      {
         max_val = (std::numeric_limits<IntType>::max)();
         digits  = std::numeric_limits<IntType>::digits;
      }
      else
      {
         max_val = IntType(1) << bits_wanted;
         digits  = bits_wanted;
      }

      unsigned bits_per_r_val = std::numeric_limits<random_type>::digits - 1;
      while ((random_type(1) << bits_per_r_val) > (gen.max)())
         --bits_per_r_val;

      unsigned terms_needed = digits / bits_per_r_val + 1;

      IntType val   = 0;
      IntType denom = 0;
      for (unsigned i = 0; i < terms_needed; ++i)
      {
         val *= (gen.max)();
         val += gen();
      }
      for (unsigned i = 0; i < terms_needed; ++i)
      {
         denom *= (gen.max)();
         denom += gen();
      }
      if (denom == 0)
         denom = 1;
      val %= max_val;
      denom %= max_val;
      return U(val, denom);
   }

   template <class U>
   const std::vector<U>& get_hetero_test_vector()
   {
      static std::vector<U> result;
      while (result.size() < a.size())
      {
         result.push_back(generate_random<U>(std::integral_constant<int, boost::multiprecision::number_category<U>::value>()));
      }
      return result;
   }

   std::vector<T>                a, b, c, small;
   static boost::random::mt19937 gen;
};

template <class N, int V>
boost::random::mt19937 tester<N, V>::gen;

inline const char* category_name(const std::integral_constant<int, boost::multiprecision::number_kind_integer>&)
{
   return "integer";
}
inline const char* category_name(const std::integral_constant<int, boost::multiprecision::number_kind_floating_point>&)
{
   return "float";
}
inline const char* category_name(const std::integral_constant<int, boost::multiprecision::number_kind_rational>&)
{
   return "rational";
}

//
// Keys in order are:
// Category
// Operator
// Type
// Precision
// Time
//
extern std::map<std::string, std::map<std::string, std::map<std::string, std::map<int, double> > > > result_table;

inline void report_result(const char* cat, const char* type, const char* op, unsigned precision, double time)
{
   std::cout << std::left << std::setw(15) << type << std::setw(10) << precision << std::setw(35) << op << time << std::endl;
   result_table[cat][op][type][precision] = time;
}

template <class Number, int N>
void test_int_ops(tester<Number, N>& t, const char* type, unsigned precision, const std::integral_constant<int, boost::multiprecision::number_kind_integer>&)
{
   const char* cat = "integer";
   report_result(cat, type, "%", precision, t.test_mod());
   report_result(cat, type, "|", precision, t.test_or());
   report_result(cat, type, "&", precision, t.test_and());
   report_result(cat, type, "^", precision, t.test_xor());
   //report_result(cat, type, "~", precision, t.test_complement());
   report_result(cat, type, "<<", precision, t.test_left_shift());
   report_result(cat, type, ">>", precision, t.test_right_shift());
   // integer ops:
   report_result(cat, type, "%(int)", precision, t.test_mod_int());
   report_result(cat, type, "|(int)", precision, t.test_or_int());
   report_result(cat, type, "&(int)", precision, t.test_and_int());
   report_result(cat, type, "^(int)", precision, t.test_xor_int());
   report_result(cat, type, "gcd", precision, t.test_gcd());
   if(precision <= 1024)
      report_result(cat, type, "powm", precision, t.test_powm());
}
template <class Number, int N, class U>
void test_int_ops(tester<Number, N>&, const char*, unsigned, const U&)
{
}

template <class Number, int N, class U>
void test_related_ops(tester<Number, N>& t, const char* type, unsigned precision, const U&, const char* cat)
{
   report_result(cat, type, "+(value_type)", precision, t.template test_add_hetero<U>());
   report_result(cat, type, "-(value_type)", precision, t.template test_subtract_hetero<U>());
   report_result(cat, type, "*(value_type)", precision, t.template test_multiply_hetero<U>());
   report_result(cat, type, "/(value_type)", precision, t.template test_divide_hetero<U>());
   report_result(cat, type, "+=(value_type)", precision, t.template test_inplace_add_hetero<U>());
   report_result(cat, type, "-=(value_type)", precision, t.template test_inplace_subtract_hetero<U>());
   report_result(cat, type, "*=(value_type)", precision, t.template test_inplace_multiply_hetero<U>());
   report_result(cat, type, "/=(value_type)", precision, t.template test_inplace_divide_hetero<U>());
}
template <class Number, int N>
void test_related_ops(tester<Number, N>& t, const char* type, unsigned precision, const Number&, const char* cat)
{
   static_cast<void>(t);
   static_cast<void>(type);
   static_cast<void>(precision);
   static_cast<void>(cat);
}

template <class Number>
void test(const char* type, unsigned precision)
{
   bits_wanted = precision;
   tester<Number, boost::multiprecision::number_category<Number>::value> t;
   const char*                                                           cat = category_name(typename boost::multiprecision::number_category<Number>::type());
   //
   // call t.test_multiply() first so that the destination operands are
   // forced to perform whatever memory allocation may be needed.  That way
   // we measure only algorithm performance, and not memory allocation effects.
   //
   t.test_multiply();
   //
   // Now the actual tests:
   //
#ifndef TEST_MUL_ONLY
   report_result(cat, type, "+", precision, t.test_add());
   report_result(cat, type, "-", precision, t.test_subtract());
#endif
   report_result(cat, type, "*", precision, t.test_multiply());
#ifndef TEST_MUL_ONLY
   report_result(cat, type, "/", precision, t.test_divide());
   report_result(cat, type, "sqrt", precision, t.test_sqrt());
   report_result(cat, type, "log", precision, t.test_log());
   report_result(cat, type, "exp", precision, t.test_exp());
   report_result(cat, type, "tan", precision, t.test_tan());
   report_result(cat, type, "str", precision, t.test_str());
   // integer ops:
   report_result(cat, type, "+(int)", precision, t.test_add_int());
   report_result(cat, type, "-(int)", precision, t.test_subtract_int());
   report_result(cat, type, "*(int)", precision, t.test_multiply_int());
   report_result(cat, type, "/(int)", precision, t.test_divide_int());
   // construction and destruction:
   report_result(cat, type, "construct", precision, t.test_construct());
   report_result(cat, type, "construct(unsigned)", precision, t.test_construct_unsigned());
   report_result(cat, type, "construct(unsigned long long)", precision, t.test_construct_unsigned_ll());
   test_int_ops(t, type, precision, typename boost::multiprecision::number_category<Number>::type());
   test_related_ops(t, type, precision, typename Number::value_type(), cat);
   // Hetero ops:
   report_result(cat, type, "+(unsigned long long)", precision, t.template test_add_hetero<unsigned long long>());
   report_result(cat, type, "-(unsigned long long)", precision, t.template test_subtract_hetero<unsigned long long>());
   report_result(cat, type, "*(unsigned long long)", precision, t.template test_multiply_hetero<unsigned long long>());
   report_result(cat, type, "/(unsigned long long)", precision, t.template test_divide_hetero<unsigned long long>());
   report_result(cat, type, "+=(unsigned long long)", precision, t.template test_inplace_add_hetero<unsigned long long>());
   report_result(cat, type, "-=(unsigned long long)", precision, t.template test_inplace_subtract_hetero<unsigned long long>());
   report_result(cat, type, "*=(unsigned long long)", precision, t.template test_inplace_multiply_hetero<unsigned long long>());
   report_result(cat, type, "/=(unsigned long long)", precision, t.template test_inplace_divide_hetero<unsigned long long>());
#endif
}

#endif // PERFORMANCE_TEST_DF_2025_01_09_HPP
