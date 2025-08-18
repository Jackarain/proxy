//  Copyright John Maddock 2007.
//  Copyright Matt Borland 2023.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/multiprecision/cpp_double_fp.hpp>
#include <boost/math/special_functions/modf.hpp>
#include <boost/math/special_functions/round.hpp>
#include <boost/math/special_functions/sign.hpp>
#include <boost/math/special_functions/trunc.hpp>

#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <type_traits>

template <class T, class U>
typename std::enable_if<!boost::multiprecision::is_interval_number<T>::value>::type check_within_half(T a, U u)
{
   BOOST_MATH_STD_USING
   const auto fabs_res {fabs(a - u)};
   if (fabs_res > 0.5f)
   {
      // LCOV_EXCL_START
      BOOST_ERROR("Rounded result differed by more than 0.5 from the original");
      std::cerr << "Values were: " << std::setprecision(35) << std::setw(40)
                << std::left << a << u << std::endl;
      // LCOV_EXCL_STOP
   }
   if ((fabs(a - u) == 0.5f) && (fabs(static_cast<T>(u)) < fabs(a)))
   {
      // LCOV_EXCL_START
      BOOST_ERROR("Rounded result was towards zero with boost::round");
      std::cerr << "Values were: " << std::setprecision(35) << std::setw(40)
                << std::left << a << u << std::endl;
      // LCOV_EXCL_STOP
   }
}
template <class T, class U>
typename std::enable_if<boost::multiprecision::is_interval_number<T>::value>::type check_within_half(T a, U u)
{
   BOOST_MATH_STD_USING
   if (upper(T(fabs(a - u))) > 0.5f)
   {
      // LCOV_EXCL_START
      BOOST_ERROR("Rounded result differed by more than 0.5 from the original");
      std::cerr << "Values were: " << std::setprecision(35) << std::setw(40)
                << std::left << a << u << std::endl;
      // LCOV_EXCL_STOP
   }
   if ((upper(T(fabs(a - u))) == 0.5f) && (fabs(static_cast<T>(u)) < fabs(a)))
   {
      // LCOV_EXCL_START
      BOOST_ERROR("Rounded result was towards zero with boost::round");
      std::cerr << "Values were: " << std::setprecision(35) << std::setw(40)
                << std::left << a << u << std::endl;
      // LCOV_EXCL_STOP
   }
}

//
// We may not have an abs overload for long long so provide a fall back:
//
inline unsigned safe_abs(int const& v)
{
   return v < 0 ? static_cast<unsigned>(1u) + static_cast<unsigned>(-(v + 1)) : v;
}
inline unsigned long safe_abs(long const& v)
{
   return v < 0 ? static_cast<unsigned long>(1u) + static_cast<unsigned long>(-(v + 1)) : v;
}
inline unsigned long long safe_abs(long long const& v)
{
   return v < 0 ? static_cast<unsigned long long>(1u) + static_cast<unsigned long long>(-(v + 1)) : v;
}
template <class T>
inline typename std::enable_if<!boost::multiprecision::detail::is_integral<T>::value, T>::type safe_abs(T const& v)
{
   return v < 0 ? -v : v;
}

template <class T, class U>
void check_trunc_result(T a, U u)
{
   BOOST_MATH_STD_USING
   if (fabs(a - u) >= 1)
   {
      // LCOV_EXCL_START
      BOOST_ERROR("Rounded result differed by more than 1 from the original");
      std::cerr << "Values were: " << std::setprecision(35) << std::setw(40)
                << std::left << a << u << std::endl;
      // LCOV_EXCL_STOP
   }
   if (abs(a) < safe_abs(u))
   {
      // LCOV_EXCL_START
      BOOST_ERROR("Truncated result had larger absolute value than the original");
      std::cerr << "Values were: " << std::setprecision(35) << std::setw(40)
                << std::left << abs(a) << safe_abs(u) << std::endl;
      // LCOV_EXCL_STOP
   }
   if (fabs(static_cast<T>(u)) > fabs(a))
   {
      // LCOV_EXCL_START
      BOOST_ERROR("Rounded result was away from zero with boost::trunc");
      std::cerr << "Values were: " << std::setprecision(35) << std::setw(40)
                << std::left << a << u << std::endl;
      // LCOV_EXCL_STOP
   }
}

template <typename T>
void test()
{
    if (std::numeric_limits<T>::digits >= std::numeric_limits<long>::digits)
    {
        auto lhs = static_cast<T>((std::numeric_limits<long>::max)());
        long k = lround(lhs);
        check_within_half(lhs, k);
        BOOST_TEST(k == lround(static_cast<T>((std::numeric_limits<long>::max)()) + 0));
        k = lround(static_cast<T>((std::numeric_limits<long>::min)()));
        check_within_half(static_cast<T>((std::numeric_limits<long>::min)()), k);
        BOOST_TEST(k == lround(static_cast<T>((std::numeric_limits<long>::min)()) + 0));
        k = ltrunc(static_cast<T>((std::numeric_limits<long>::max)()));
        check_trunc_result(static_cast<T>((std::numeric_limits<long>::max)()), k);
        BOOST_TEST(k == ltrunc(static_cast<T>((std::numeric_limits<long>::max)()) + 0));
        k = ltrunc(static_cast<T>((std::numeric_limits<long>::min)()));
        check_trunc_result(static_cast<T>((std::numeric_limits<long>::min)()), k);
        BOOST_TEST(k == ltrunc(static_cast<T>((std::numeric_limits<long>::min)()) + 0));

        lhs = static_cast<T>((std::numeric_limits<long>::max)() - 1);
        k = lround(lhs);
        check_within_half(lhs, k);
        k = lround(static_cast<T>((std::numeric_limits<long>::min)() + 1));
        check_within_half(static_cast<T>((std::numeric_limits<long>::min)() + 1), k);
        k = ltrunc(static_cast<T>((std::numeric_limits<long>::max)() - 1));
        check_trunc_result(static_cast<T>((std::numeric_limits<long>::max)() - 1), k);
        k = ltrunc(static_cast<T>((std::numeric_limits<long>::min)() + 1));
        check_trunc_result(static_cast<T>((std::numeric_limits<long>::min)() + 1), k);

        // Further testing of the boundary since the upper end of long is not exactly representable by double
        for (long i = 2; i < 256; ++i)
        {
           lhs = static_cast<T>((std::numeric_limits<long>::max)() - i);
           k = lround(lhs);
           check_within_half(lhs, k);
        }
    }
}

int main()
{
   test<boost::multiprecision::cpp_double_float>();
   test<boost::multiprecision::cpp_double_double>();

   // Older PPC64LE have ibm128 instead of ieee128 which is not compatible with this type
   #if !defined(__PPC__)
   test<boost::multiprecision::cpp_double_long_double>();
   #endif

   return boost::report_errors();
}
