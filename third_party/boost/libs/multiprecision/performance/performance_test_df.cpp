///////////////////////////////////////////////////////////////
//  Copyright John Maddock 2019.
//  Copyright Christopher Kormanyos 2021 - 2025.
//  Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt

// This version of "performance_test_df.cpp" is based almost entirely on
// the original "performance_test.cpp". But it has been specifically
// adapted to test various backends in small precision ranges in order
// to check the performance comparison with cpp_double_double.

#include <boost/config.hpp>

#if !defined(TEST_CPP_DOUBLE_FLOAT)
#define TEST_CPP_DOUBLE_FLOAT
#endif

#if !defined(TEST_CPP_BIN_FLOAT)
#define TEST_CPP_BIN_FLOAT
#endif

#if !defined(TEST_CPP_DEC_FLOAT)
#define TEST_CPP_DEC_FLOAT
#endif

#if defined(BOOST_HAS_FLOAT128)
#if !defined(TEST_FLOAT128)
#define TEST_FLOAT128
#endif
#endif

#include <boost/version.hpp>

#if defined(TEST_CPP_DOUBLE_FLOAT)
#include <boost/multiprecision/cpp_double_fp.hpp>
#endif

#if defined(TEST_CPP_BIN_FLOAT)
#include <boost/multiprecision/cpp_bin_float.hpp>
#endif

#if defined(TEST_CPP_BIN_FLOAT)
#include <boost/multiprecision/cpp_dec_float.hpp>
#endif

#if defined(TEST_FLOAT128)
#include <boost/multiprecision/float128.hpp>
#endif

#include "performance_test_df.hpp"

// cd /mnt/c/Users/ckorm/Documents/Ks/PC_Software/Test
// g++ -std=gnu++14 -Wall -Wextra -O3 -I/mnt/c/ChrisGitRepos/boost_gsoc2021/multiprecision/performance -I/mnt/c/ChrisGitRepos/boost_gsoc2021/multiprecision/include -I/mnt/c/boost/boost_1_87_0 ./test.cpp -lquadmath -o test

//
// Keys in order are:
// Category
// Operator
// Type
// Precision
// Time
//
std::map<std::string, std::map<std::string, std::map<std::string, std::map<int, double> > > > result_table;

unsigned bits_wanted; // for integer types

void quickbook_results()
{
   //
   // Keys in order are:
   // Category
   // Operator
   // Type
   // Precision
   // Time
   //
   typedef std::map<std::string, std::map<std::string, std::map<std::string, std::map<int, double> > > >::const_iterator category_iterator;
   typedef std::map<std::string, std::map<std::string, std::map<int, double> > >::const_iterator                         operator_iterator;
   typedef std::map<std::string, std::map<int, double> >::const_iterator                                                 type_iterator;
   typedef std::map<int, double>::const_iterator                                                                         precision_iterator;

   for (category_iterator i = result_table.begin(); i != result_table.end(); ++i)
   {
      std::string cat = i->first;
      cat[0]          = (char)std::toupper((char)cat[0]);
      std::cout << "[section:" << i->first << "_performance " << cat << " Type Perfomance]" << std::endl;

      for (operator_iterator j = i->second.begin(); j != i->second.end(); ++j)
      {
         std::string op = j->first;
         std::cout << "[table Operator " << op << std::endl;
         std::cout << "[[Backend]";

         for (precision_iterator k = j->second.begin()->second.begin(); k != j->second.begin()->second.end(); ++k)
         {
            std::cout << "[" << k->first << " Bits]";
         }
         std::cout << "]\n";

         std::vector<double> best_times(j->second.begin()->second.size(), (std::numeric_limits<double>::max)());
         for (unsigned m = 0; m < j->second.begin()->second.size(); ++m)
         {
            for (type_iterator k = j->second.begin(); k != j->second.end(); ++k)
            {
               if (m < k->second.size())
               {
                  precision_iterator l = k->second.begin();
                  std::advance(l, m);
                  if (best_times[m] > l->second)
                     best_times[m] = l->second ? l->second : best_times[m];
               }
            }
         }

         for (type_iterator k = j->second.begin(); k != j->second.end(); ++k)
         {
            std::cout << "[[" << k->first << "]";

            unsigned m = 0;
            for (precision_iterator l = k->second.begin(); l != k->second.end(); ++l)
            {
               double rel_time = l->second / best_times[m];
               if (rel_time == 1)
                  std::cout << "[[*" << rel_time << "]";
               else
                  std::cout << "[" << rel_time;
               std::cout << " (" << l->second << "s)]";
               ++m;
            }

            std::cout << "]\n";
         }

         std::cout << "]\n";
      }

      std::cout << "[endsect]" << std::endl;
   }
}

#if defined(__HAS_INCLUDE)
#if __has_include(<sys/utsname.h>)
#define HAS_UTSNAME
#include <sys/utsname.h>
#endif
#endif
#ifdef _WIN32
#include <windows.h>
#endif

void quickbook_platform_details()
{
   std::cout << "[table:platform Platform Details\n[[Platform][";
#ifdef HAS_UTSNAME
   utsname name;
   uname(&name);
   std::cout << name.sysname << " " << name.release << ", version " << name.version << ", " << name.machine << "]]\n";
#elif defined(_WIN32)
   std::cout << "Windows ";
#ifdef _M_AMD64
   std::cout << "x64";
#elif defined(_M_IX86)
   std::cout << "x86";
#endif
   std::cout << "]]\n";
#endif
   std::cout << "[[Compiler][" << BOOST_COMPILER << "]]\n";
   std::cout << "[[Boost][" << BOOST_VERSION << "]]\n";
   std::cout << "[[Run date][" << __DATE__ << "]]\n";
   std::cout << "]\n\n";
}

namespace local {

void test29();
void test32();
void test52();
void test53();

#ifdef TEST_CPP_DOUBLE_FLOAT
template<class T>
constexpr inline auto (max)(T a, T b) noexcept -> T { return a > b ? a : b; }
#endif

} // namespace local

int main()
{
   quickbook_platform_details();

   local::test29();
   local::test32();
   local::test52();
   #if defined(BOOST_MP_CPP_DOUBLE_FP_HAS_FLOAT128)
   local::test53();
   #endif

   quickbook_results();
}

#ifdef TEST_CPP_DOUBLE_FLOAT
using double_float_of_double_type = boost::multiprecision::cpp_double_double;

constexpr auto digits10_for_performance_test = (local::max)(std::numeric_limits<double_float_of_double_type>::digits10, 32);
#else
constexpr auto digits10_for_performance_test = 32;
#endif

void local::test29()
{
#ifdef TEST_CPP_DEC_FLOAT
   static_assert(digits10_for_performance_test >= 32, "Error: Too few digits for performance comparison");

   using cpp_dec_float_type =
      boost::multiprecision::number<boost::multiprecision::backends::cpp_dec_float<digits10_for_performance_test>,
                                    boost::multiprecision::et_off>;

   test<cpp_dec_float_type>("cpp_dec_float", digits10_for_performance_test);
#endif
}

void local::test32()
{
#ifdef TEST_CPP_BIN_FLOAT
   static_assert(digits10_for_performance_test >= 32, "Error: Too few digits for performance comparison");

   using cpp_bin_float_type =
      boost::multiprecision::number<boost::multiprecision::backends::cpp_bin_float<digits10_for_performance_test>,
                                    boost::multiprecision::et_off>;

   test<cpp_bin_float_type>("cpp_bin_float", digits10_for_performance_test);
#endif
}

void local::test52()
{
#ifdef TEST_CPP_DOUBLE_FLOAT
   test<double_float_of_double_type>("cpp_double_fp_backend<double>", digits10_for_performance_test);
#endif
}

void local::test53()
{
#if defined(BOOST_HAS_FLOAT128)
#ifdef TEST_FLOAT128
   using boost::multiprecision::float128;

   test<float128>("boost::multiprecision::float128(at)", std::numeric_limits<boost::multiprecision::float128>::digits10);
#endif
#endif
}

#if 0

See also: https://github.com/BoostGSoC21/multiprecision/issues/178#issuecomment-2580157139

## section:float_performance Float Type Perfomance

| Operation                       | `float128`           |`cpp_bin_float<32>`     | `cpp_dec_float<32>`    | `cpp_double_fp_backend<double>` |
|---------------------------------|----------------------|------------------------|------------------------|---------------------------------|
| `*`                             | 1.45209 (0.0530208s) | 1.4866 (0.054281s)     | 4.38403 (0.160076s)    | [*1] (0.0365135s)      |
| `*(int)`                        | 1.54876 (0.0396009s) | 2.08722 (0.0533689s)   | 5.97016 (0.152653s)    | [*1] (0.0255694s)      |
| `*(unsigned long long)`         | 1.66579 (0.083952s)  | 1.109 (0.0558912s)     | 3.64263 (0.18358s)     | [*1] (0.0503976s)      |
| `*=(unsigned long long)`        | 1.70531 (0.0858822s) | 1.06861 (0.053817s)    | 3.47129 (0.17482s)     | [*1] (0.0503617s)      |
| `+`                             | 2.4858 (0.0430852s)  | 5.38736 (0.0933768s)   | 3.06832 (0.0531818s)   | [*1] (0.0173326s)      |
| `+(int)`                        | 2.96871 (0.0360276s) | 7.95354 (0.0965222s)   | 5.97545 (0.0725166s)   | [*1] (0.0121358s)      |
| `+(unsigned long long)`         | 3.38383 (0.0739771s) | 5.51557 (0.120581s)    | 4.42454 (0.0967292s)   | [*1] (0.021862s)       |
| `+=(unsigned long long)`        | 3.3944 (0.0760423s)  | 4.98724 (0.111726s)    | 3.92369 (0.0878995s)   | [*1] (0.0224023s)      |
| `-`                             | 2.20087 (0.0389465s) | 5.40743 (0.0956897s)   | 3.24191 (0.0573686s)   | [*1] (0.0176959s)      |
| `-(int)`                        | 3.21093 (0.0383358s) | 8.65589 (0.103344s)    | 7.53768 (0.0899936s)   | [*1] (0.0119392s)      |
| `-(unsigned long long)`         | 3.45536 (0.075553s)  | 5.17747 (0.113208s)    | 4.63221 (0.101285s)    | [*1] (0.0218654s)      |
| `-=(unsigned long long)`        | 2.73635 (0.0764208s) | 3.773 (0.105372s)      | 3.39784 (0.0948951s)   | [*1] (0.027928s)       |
| `/`                             | 1.38437 (0.148412s)  | 7.55692 (0.810141s)    | 23.5198 (2.52144s)     | [*1] (0.107205s)       |
| `/(int)`                        | 1.53126 (0.0384472s) | 6.85045 (0.172002s)    | 49.004 (1.2304s)       | [*1] (0.0251081s)      |
| `/(unsigned long long)`         | 2.41876 (0.098966s)  | 9.80113 (0.401023s)    | 30.6916 (1.25578s)     | [*1] (0.040916s)       |
| `/=(unsigned long long)`        | 1.88263 (0.0994829s) | 7.33976 (0.387852s)    | 23.6024 (1.24721s)     | [*1] (0.0528426s)      |
| `construct`                     | [*1] (0.000550501s)  | 4.74042 (0.00260961s)  | 13.0381 (0.00717748s)  | 3.51129 (0.00193297s)  |
| `construct(unsigned long long)` | 5.33373 (0.0401408s) | 2.82475 (0.0212586s)   | 3.38482 (0.0254736s)   | [*1] (0.00752583s)     |
| `construct(unsigned)`           | 23.3495 (0.0403851s) | 12.4928 (0.0216075s)   | 14.3869 (0.0248834s)   | [*1] (0.00172959s)     |
| `exp`                           | 1.37724 (1.66978s)   | 5.45487 (6.61357s)     | 5.19473 (6.29817s)     | [*1] (1.21242s)        |
| `log`                           | [*1] (2.1149s)       | 7.38874 (15.6264s)     | 27.0206 (57.146s)      | 1.10118 (2.32888s)     |
| `sqrt`                          | 25.9413 (0.53951s)   | 24.5484 (0.510541s)    | 131.925 (2.74368s)     | [*1] (0.0207973s)      |
| `str`                           | [*1] (0.000419145s)  | 1.50425 (0.000630497s) | 1.22795 (0.000514689s) | 1.82207 (0.000763711s) |
| `tan`                           | 1.3995 (1.69463s)    | 5.46528 (6.61782s)     | 5.20608 (6.30396s)     | [*1] (1.21088s)        |

#endif
