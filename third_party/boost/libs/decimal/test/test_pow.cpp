// Copyright 2023 - 2026 Matt Borland
// Copyright 2023 - 2026 Christopher Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include "testing_config.hpp"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>

#include <boost/decimal.hpp>

#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wfloat-equal"
#elif defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#endif
#include <boost/core/lightweight_test.hpp>

template<typename DecimalType> auto my_zero() -> DecimalType&;
template<typename DecimalType> auto my_one () -> DecimalType&;
template<typename DecimalType> auto my_ten () -> DecimalType&;
template<typename DecimalType> auto my_inf () -> DecimalType&;
template<typename DecimalType> auto my_nan () -> DecimalType&;

namespace local
{
  template<typename IntegralTimePointType,
           typename ClockType = std::chrono::high_resolution_clock>
  auto time_point() noexcept -> IntegralTimePointType
  {
    using local_integral_time_point_type = IntegralTimePointType;
    using local_clock_type               = ClockType;

    const auto current_now =
      static_cast<std::uintmax_t>
      (
        std::chrono::duration_cast<std::chrono::nanoseconds>
        (
          local_clock_type::now().time_since_epoch()
        ).count()
      );

    return static_cast<local_integral_time_point_type>(current_now);
  }

  template<typename NumericType>
  auto is_close_fraction(const NumericType& a,
                         const NumericType& b,
                         const NumericType& tol) noexcept -> bool
  {
    using std::fabs;

    auto result_is_ok = bool { };

    if(b == static_cast<NumericType>(0))
    {
      result_is_ok = (fabs(a - b) < tol); // LCOV_EXCL_LINE
    }
    else
    {
      const auto delta = fabs(1 - (a / b));

      result_is_ok = (delta < tol);
    }

    return result_is_ok;
  }

  template<typename DecimalType, typename FloatType>
  auto test_pow(const int tol_factor, const bool neg_a) -> bool
  {
    using decimal_type = DecimalType;
    using float_type   = FloatType;

    std::random_device rd;
    std::mt19937_64 gen(rd());

    gen.seed(time_point<typename std::mt19937_64::result_type>());

    auto dis_x =
      std::uniform_real_distribution<float_type>
      {
        static_cast<float_type>(1.0E-1L),
        static_cast<float_type>(1.0E+1L)
      };

    auto dis_a =
      std::uniform_real_distribution<float_type>
      {
        static_cast<float_type>(0.0123L),
        static_cast<float_type>(12.3L)
      };

    auto result_is_ok = true;

    auto trials = static_cast<std::uint32_t>(UINT8_C(0));

    #if !defined(BOOST_DECIMAL_REDUCE_TEST_DEPTH)
    constexpr auto count = (sizeof(decimal_type) == static_cast<std::size_t>(UINT8_C(4))) ? UINT32_C(0x400) : UINT32_C(0x40);
    #else
    constexpr auto count = (sizeof(decimal_type) == static_cast<std::size_t>(UINT8_C(4))) ? UINT32_C(0x40) : UINT32_C(0x4);
    #endif

    for( ; trials < count; ++trials)
    {
      float_type a_flt_begin { };
      float_type x_flt_begin { };

      do { a_flt_begin = dis_a(gen); } while(a_flt_begin == 0);
      do { x_flt_begin = dis_x(gen); } while(x_flt_begin == 0);

      const auto a_flt = ((!neg_a) ? a_flt_begin : -a_flt_begin);
      const auto a_dec = static_cast<decimal_type>(a_flt);

      const auto x_flt = x_flt_begin;
      const auto x_dec = static_cast<decimal_type>(x_flt);

      using std::pow;

      const auto val_flt = pow(x_flt, a_flt);
      const auto val_dec = pow(x_dec, a_dec);

      const auto result_val_is_ok = is_close_fraction(val_flt, static_cast<float_type>(val_dec), std::numeric_limits<float_type>::epsilon() * static_cast<float_type>(tol_factor));

      result_is_ok = (result_val_is_ok && result_is_ok);

      BOOST_TEST(result_val_is_ok);

      if(!result_val_is_ok)
      //LCOV_EXCL_START
      {
        std::stringstream strm;

        strm <<   "x_flt  : " << std::scientific << std::setprecision(std::numeric_limits<float_type>::digits10) << x_flt
             << "\na_flt  : " << std::scientific << std::setprecision(std::numeric_limits<float_type>::digits10) << a_flt
             << "\nval_flt: " << std::scientific << std::setprecision(std::numeric_limits<float_type>::digits10) << val_flt
             << "\nval_dec: " << std::scientific << std::setprecision(std::numeric_limits<float_type>::digits10) << val_dec;

        std::cerr << strm.str();
      }
      // LCOV_EXCL_STOP
    }

    BOOST_TEST(result_is_ok);

    return result_is_ok;
  }

  template<typename DecimalType, typename FloatType>
  auto test_pow_n(const int tol_factor) -> bool
  {
    using decimal_type = DecimalType;
    using float_type   = FloatType;

    auto result_is_ok = true;

    {
      using std::acos;

      const auto near_pi_flt = acos(static_cast<float_type>(-1.0L));
      const auto near_pi_dec = boost::decimal::numbers::pi_v<decimal_type>;

      for(int p = 3; p < 12; ++p)
      {
        using std::pow;

        const auto val_flt = pow(near_pi_flt, static_cast<float_type>  (p));
        const auto val_dec = pow(near_pi_dec, static_cast<decimal_type>(p));

        const auto result_val_is_ok = is_close_fraction(val_flt, static_cast<float_type>(val_dec), std::numeric_limits<float_type>::epsilon() * static_cast<float_type>(tol_factor));

        result_is_ok = (result_val_is_ok && result_is_ok);

        if(!result_val_is_ok)
        {
          // LCOV_EXCL_START
          std::cout << "p:       " << std::scientific << p   << std::endl;
          std::cout << "val_flt: " << std::scientific << std::setprecision(std::numeric_limits<float_type>::digits10) << val_flt << std::endl;
          std::cout << "val_dec: " << std::scientific << std::setprecision(std::numeric_limits<float_type>::digits10) << val_dec << std::endl;

          break;
          // LCOV_EXCL_STOP
        }
      }

      BOOST_TEST(result_is_ok);

      for(int p = -11; p <= -3; ++p)
      {
        using std::pow;

        const auto val_flt = pow(near_pi_flt, static_cast<float_type>  (p));
        const auto val_dec = pow(near_pi_dec, static_cast<decimal_type>(p));

        const auto result_val_is_ok = is_close_fraction(val_flt, static_cast<float_type>(val_dec), std::numeric_limits<float_type>::epsilon() * static_cast<float_type>(tol_factor));

        result_is_ok = (result_val_is_ok && result_is_ok);

        if(!result_val_is_ok)
        {
          // LCOV_EXCL_START
          std::cout << "p:       " << std::scientific << p   << std::endl;
          std::cout << "val_flt: " << std::scientific << std::setprecision(std::numeric_limits<float_type>::digits10) << val_flt << std::endl;
          std::cout << "val_dec: " << std::scientific << std::setprecision(std::numeric_limits<float_type>::digits10) << val_dec << std::endl;

          break;
          // LCOV_EXCL_STOP
        }
      }

      BOOST_TEST(result_is_ok);
    }

    constexpr decimal_type one { 1, 0 };

    {
      using std::acos;

      const auto inv_pi_flt = static_cast<float_type>(1.0L) / acos(static_cast<float_type>(-1.0L));
      const auto inv_pi_dec = one / boost::decimal::numbers::pi_v<decimal_type>;

      for(int p = 3; p < 12; ++p)
      {
        using std::pow;

        const auto val_flt = pow(inv_pi_flt, static_cast<float_type>  (p));
        const auto val_dec = pow(inv_pi_dec, static_cast<decimal_type>(p));

        const auto result_val_is_ok = is_close_fraction(val_flt, static_cast<float_type>(val_dec), std::numeric_limits<float_type>::epsilon() * static_cast<float_type>(tol_factor));

        result_is_ok = (result_val_is_ok && result_is_ok);

        if(!result_val_is_ok)
        {
          // LCOV_EXCL_START
          std::cout << "p:       " << std::scientific << p   << std::endl;
          std::cout << "val_flt: " << std::scientific << std::setprecision(std::numeric_limits<float_type>::digits10) << val_flt << std::endl;
          std::cout << "val_dec: " << std::scientific << std::setprecision(std::numeric_limits<float_type>::digits10) << val_dec << std::endl;

          break;
          // LCOV_EXCL_STOP
        }
      }

      BOOST_TEST(result_is_ok);

      for(auto p = -11; p <= -3; ++p)
      {
        using std::pow;

        const auto val_flt = pow(inv_pi_flt, static_cast<float_type>  (p));
        const auto val_dec = pow(inv_pi_dec, p);

        const auto result_val_is_ok = is_close_fraction(val_flt, static_cast<float_type>(val_dec), std::numeric_limits<float_type>::epsilon() * static_cast<float_type>(tol_factor));

        result_is_ok = (result_val_is_ok && result_is_ok);

        if(!result_val_is_ok)
        {
          // LCOV_EXCL_START
          std::cout << "p:       " << std::scientific << p   << std::endl;
          std::cout << "val_flt: " << std::scientific << std::setprecision(std::numeric_limits<float_type>::digits10) << val_flt << std::endl;
          std::cout << "val_dec: " << std::scientific << std::setprecision(std::numeric_limits<float_type>::digits10) << val_dec << std::endl;

          break;
          // LCOV_EXCL_STOP
        }
      }

      BOOST_TEST(result_is_ok);
    }

    {
      using std::acos;

      const auto neg_pi_flt = -acos(static_cast<float_type>(-1.0L));
      const auto neg_pi_dec = -boost::decimal::numbers::pi_v<decimal_type>;

      for(auto p = 3U; p < 12U; ++p)
      {
        using std::pow;

        const auto val_flt = pow(neg_pi_flt, static_cast<float_type>  (p));
        const auto val_dec = pow(neg_pi_dec, p);

        const auto result_val_is_ok = is_close_fraction(val_flt, static_cast<float_type>(val_dec), std::numeric_limits<float_type>::epsilon() * static_cast<float_type>(tol_factor));

        result_is_ok = (result_val_is_ok && result_is_ok);

        if(!result_val_is_ok)
        {
          // LCOV_EXCL_START
          std::cout << "p:       " << std::scientific << p   << std::endl;
          std::cout << "val_flt: " << std::scientific << std::setprecision(std::numeric_limits<float_type>::digits10) << val_flt << std::endl;
          std::cout << "val_dec: " << std::scientific << std::setprecision(std::numeric_limits<float_type>::digits10) << val_dec << std::endl;

          break;
          // LCOV_EXCL_STOP
        }
      }

      BOOST_TEST(result_is_ok);
    }

    return result_is_ok;
  }

  template<typename DecimalType>
  auto test_10_pow_n() -> bool
  {
    using decimal_type = DecimalType;

    using ctrl_val_array_type = std::array<decimal_type, static_cast<std::size_t>(UINT8_C(5))>;

    bool result_is_ok { true };

    {
      const ctrl_val_array_type ctrl_values =
      {{
        decimal_type { 1,  1},
        decimal_type { 1,  4},
        decimal_type { 1,  9},
        decimal_type { 1, 16},
        decimal_type { 1, 25},
      }};

      for(auto i = static_cast<std::size_t>(UINT8_C(0)); i < std::tuple_size<ctrl_val_array_type>::value; ++i)
      {
        const int n = static_cast<int>(i + static_cast<std::size_t>(UINT8_C(1)));

        decimal_type p10 = pow(pow(::my_ten<decimal_type>(), n), n);

        const bool result_p10_is_ok = (p10 == ctrl_values[i]);

        result_is_ok = (result_p10_is_ok && result_is_ok);
      }
    }

    {
      const ctrl_val_array_type ctrl_values =
      {{
        decimal_type { 1,  -1},
        decimal_type { 1,  -4},
        decimal_type { 1,  -9},
        decimal_type { 1, -16},
        decimal_type { 1, -25},
      }};

      for(auto i = static_cast<std::size_t>(UINT8_C(0)); i < std::tuple_size<ctrl_val_array_type>::value; ++i)
      {
        const int n = static_cast<int>(i + static_cast<std::size_t>(UINT8_C(1)));

        decimal_type p10 = pow(pow(::my_ten<decimal_type>(), -n), n);

        const bool result_p10_is_ok = (p10 == ctrl_values[i]);

        result_is_ok = (result_p10_is_ok && result_is_ok);
      }
    }

    BOOST_TEST(result_is_ok);

    return result_is_ok;
  }

  template<typename DecimalType, typename FloatType>
  auto test_pow_edge() -> bool
  {
    using decimal_type = DecimalType;
    using float_type   = FloatType;

    std::mt19937_64 gen;

    gen.seed(time_point<typename std::mt19937_64::result_type>());

    std::uniform_real_distribution<float_type>
      dist
      (
        static_cast<float_type>(1.01L),
        static_cast<float_type>(1.04L)
      );

    auto dist_x =
      std::uniform_real_distribution<float_type>
      {
        static_cast<float_type>(1.0E-1L),
        static_cast<float_type>(1.0E+1L)
      };

    auto dist_a =
      std::uniform_real_distribution<float_type>
      {
        static_cast<float_type>(0.0123L),
        static_cast<float_type>(12.3L)
      };

    constexpr decimal_type one { 1, 0 };

    auto result_is_ok = true;

    using std::pow;

    for(auto index = 0U; index < 8U; ++index)
    {
      static_cast<void>(index);

      const auto flt_near_one = dist(gen);

      const auto dec_pos_pos = pow( ::my_nan<decimal_type>() * static_cast<decimal_type>(flt_near_one),  ::my_zero<decimal_type>() * static_cast<decimal_type>(dist(gen)));
      const auto dec_pos_neg = pow( ::my_nan<decimal_type>() * static_cast<decimal_type>(flt_near_one), -::my_zero<decimal_type>() * static_cast<decimal_type>(dist(gen)));
      const auto dec_neg_pos = pow(-::my_nan<decimal_type>() * static_cast<decimal_type>(flt_near_one),  ::my_zero<decimal_type>() * static_cast<decimal_type>(dist(gen)));
      const auto dec_neg_neg = pow(-::my_nan<decimal_type>() * static_cast<decimal_type>(flt_near_one), -::my_zero<decimal_type>() * static_cast<decimal_type>(dist(gen)));

      const auto flt_pos_pos = pow( std::numeric_limits<float_type>::quiet_NaN() * flt_near_one,  static_cast<float_type>(0.0L));
      const auto flt_pos_neg = pow( std::numeric_limits<float_type>::quiet_NaN() * flt_near_one, -static_cast<float_type>(0.0L));
      const auto flt_neg_pos = pow(-std::numeric_limits<float_type>::quiet_NaN() * flt_near_one,  static_cast<float_type>(0.0L));
      const auto flt_neg_neg = pow(-std::numeric_limits<float_type>::quiet_NaN() * flt_near_one, -static_cast<float_type>(0.0L));

      const auto result_pow_is_ok =
        (
             ((dec_pos_pos == one) == (flt_pos_pos == static_cast<float_type>(1.0L)))
          && ((dec_pos_neg == one) == (flt_pos_neg == static_cast<float_type>(1.0L)))
          && ((dec_neg_pos == one) == (flt_neg_pos == static_cast<float_type>(1.0L)))
          && ((dec_neg_neg == one) == (flt_neg_neg == static_cast<float_type>(1.0L)))
        );

      BOOST_TEST(result_pow_is_ok);

      result_is_ok = (result_pow_is_ok && result_is_ok);
    }

    for(auto index = 0U; index < 8U; ++index)
    {
      static_cast<void>(index);

      const auto flt_near_one = dist(gen);

      const auto dec_pos_pos = pow( std::numeric_limits<decimal_type>::infinity() * static_cast<decimal_type>(flt_near_one),  ::my_zero<decimal_type>());
      const auto dec_pos_neg = pow( std::numeric_limits<decimal_type>::infinity() * static_cast<decimal_type>(flt_near_one), -::my_zero<decimal_type>());
      const auto dec_neg_pos = pow(-std::numeric_limits<decimal_type>::infinity() * static_cast<decimal_type>(flt_near_one),  ::my_zero<decimal_type>());
      const auto dec_neg_neg = pow(-std::numeric_limits<decimal_type>::infinity() * static_cast<decimal_type>(flt_near_one), -::my_zero<decimal_type>());

      const auto flt_pos_pos = pow( std::numeric_limits<float_type>::infinity() * flt_near_one,  static_cast<float_type>(0.0L));
      const auto flt_pos_neg = pow( std::numeric_limits<float_type>::infinity() * flt_near_one, -static_cast<float_type>(0.0L));
      const auto flt_neg_pos = pow(-std::numeric_limits<float_type>::infinity() * flt_near_one,  static_cast<float_type>(0.0L));
      const auto flt_neg_neg = pow(-std::numeric_limits<float_type>::infinity() * flt_near_one, -static_cast<float_type>(0.0L));

      const auto result_pow_is_ok =
        (
             ((dec_pos_pos == one) == (flt_pos_pos == static_cast<float_type>(1.0L)))
          && ((dec_pos_neg == one) == (flt_pos_neg == static_cast<float_type>(1.0L)))
          && ((dec_neg_pos == one) == (flt_neg_pos == static_cast<float_type>(1.0L)))
          && ((dec_neg_neg == one) == (flt_neg_neg == static_cast<float_type>(1.0L)))
        );

      BOOST_TEST(result_pow_is_ok);

      result_is_ok = (result_pow_is_ok && result_is_ok);
    }

    for(auto index = 0U; index < 8U; ++index)
    {
      static_cast<void>(index);

      const auto flt_x = dist_x(gen);

      const auto dec_pos_pos = pow(static_cast<decimal_type>( flt_x),  ::my_zero<decimal_type>() * static_cast<decimal_type>(dist(gen)));
      const auto dec_pos_neg = pow(static_cast<decimal_type>( flt_x), -::my_zero<decimal_type>() * static_cast<decimal_type>(dist(gen)));
      const auto dec_neg_pos = pow(static_cast<decimal_type>(-flt_x),  ::my_zero<decimal_type>() * static_cast<decimal_type>(dist(gen)));
      const auto dec_neg_neg = pow(static_cast<decimal_type>(-flt_x), -::my_zero<decimal_type>() * static_cast<decimal_type>(dist(gen)));

      const auto flt_pos_pos = pow( flt_x,  static_cast<float_type>(0.0L));
      const auto flt_pos_neg = pow( flt_x, -static_cast<float_type>(0.0L));
      const auto flt_neg_pos = pow(-flt_x,  static_cast<float_type>(0.0L));
      const auto flt_neg_neg = pow(-flt_x, -static_cast<float_type>(0.0L));

      const auto result_pow_is_ok =
        (
             ((dec_pos_pos == one) == (flt_pos_pos == static_cast<float_type>(1.0L)))
          && ((dec_pos_neg == one) == (flt_pos_neg == static_cast<float_type>(1.0L)))
          && ((dec_neg_pos == one) == (flt_neg_pos == static_cast<float_type>(1.0L)))
          && ((dec_neg_neg == one) == (flt_neg_neg == static_cast<float_type>(1.0L)))
        );

      BOOST_TEST(result_pow_is_ok);

      result_is_ok = (result_pow_is_ok && result_is_ok);
    }

    using std::isinf;

    for(auto index = 0U; index < 8U; ++index)
    {
      static_cast<void>(index);

      const float_type flt_a = dist_a(gen);

      const auto dec_nrm_pos = pow(::my_zero<decimal_type>() * static_cast<decimal_type>(dist(gen)),  static_cast<decimal_type>(flt_a));
      const auto dec_nrm_neg = pow(::my_zero<decimal_type>() * static_cast<decimal_type>(dist(gen)), -static_cast<decimal_type>(flt_a));
      const auto dec_inf_pos = pow(::my_zero<decimal_type>() * static_cast<decimal_type>(dist(gen)),  std::numeric_limits<decimal_type>::infinity());
      const auto dec_inf_neg = pow(::my_zero<decimal_type>() * static_cast<decimal_type>(dist(gen)), -std::numeric_limits<decimal_type>::infinity());

      const auto flt_nrm_pos = pow(static_cast<float_type>(0.0L),  flt_a);
      const auto flt_nrm_neg = pow(static_cast<float_type>(0.0L), -flt_a);
      const auto flt_inf_pos = pow(static_cast<float_type>(0.0L),  std::numeric_limits<float_type>::infinity());
      const auto flt_inf_neg = pow(static_cast<float_type>(0.0L), -std::numeric_limits<float_type>::infinity());

      const auto result_pow_is_ok =
        (
             (     (dec_nrm_pos  == ::my_zero<decimal_type>()) ==      (flt_nrm_pos == static_cast<float_type>(0.0L)))
          && (isinf(dec_nrm_neg)                               == isinf(flt_nrm_neg))
          && (     (dec_inf_pos  == ::my_zero<decimal_type>()) ==      (flt_inf_pos == static_cast<float_type>(0.0L)))
          && (isinf(dec_inf_neg)                               == isinf(flt_inf_neg))
        );

      BOOST_TEST(result_pow_is_ok);

      result_is_ok = (result_pow_is_ok && result_is_ok);
    }

    for(auto index = 0U; index < 8U; ++index)
    {
      using std::fpclassify;

      static_cast<void>(index);

      const auto flt_a = dist_a(gen);

      const auto dec_pos_pos = pow( ::my_zero<decimal_type>(), ::my_nan<decimal_type>() * static_cast<decimal_type>(flt_a));
      const auto dec_neg_pos = pow(-::my_zero<decimal_type>(), ::my_nan<decimal_type>() * static_cast<decimal_type>(flt_a));

      const auto flt_pos_pos = pow( static_cast<float_type>(0.0L), std::numeric_limits<float_type>::quiet_NaN() * flt_a);
      const auto flt_neg_pos = pow(-static_cast<float_type>(0.0L), std::numeric_limits<float_type>::quiet_NaN() * flt_a);

      const auto result_pow_is_ok =
        (
             ((fpclassify(dec_pos_pos) == FP_NAN) == (fpclassify(flt_pos_pos) == FP_NAN))
          && ((fpclassify(dec_neg_pos) == FP_NAN) == (fpclassify(flt_neg_pos) == FP_NAN))
        );

      BOOST_TEST(result_pow_is_ok);

      result_is_ok = (result_pow_is_ok && result_is_ok);
    }

    for(auto index = 0U; index < 4U; ++index)
    {
      static_cast<void>(index);

      const auto flt_x = dist_x(gen);

      const auto dec_pos_pos = pow( static_cast<decimal_type>(flt_x),  ::my_zero<decimal_type>() * static_cast<decimal_type>(dist(gen)));
      const auto dec_pos_neg = pow( static_cast<decimal_type>(flt_x), -::my_zero<decimal_type>() * static_cast<decimal_type>(dist(gen)));
      const auto dec_neg_pos = pow(-static_cast<decimal_type>(flt_x),  ::my_zero<decimal_type>() * static_cast<decimal_type>(dist(gen)));
      const auto dec_neg_neg = pow(-static_cast<decimal_type>(flt_x), -::my_zero<decimal_type>() * static_cast<decimal_type>(dist(gen)));

      const auto flt_pos_pos = pow( flt_x,  static_cast<float_type>(0.0L));
      const auto flt_pos_neg = pow( flt_x, -static_cast<float_type>(0.0L));
      const auto flt_neg_pos = pow(-flt_x,  static_cast<float_type>(0.0L));
      const auto flt_neg_neg = pow(-flt_x, -static_cast<float_type>(0.0L));

      const auto result_pow_is_ok =
        (
             ((dec_pos_pos == one) == (flt_pos_pos == static_cast<float_type>(1.0L)))
          && ((dec_pos_neg == one) == (flt_pos_neg == static_cast<float_type>(1.0L)))
          && ((dec_neg_pos == one) == (flt_neg_pos == static_cast<float_type>(1.0L)))
          && ((dec_neg_neg == one) == (flt_neg_neg == static_cast<float_type>(1.0L)))
        );

      BOOST_TEST(result_pow_is_ok);

      result_is_ok = (result_pow_is_ok && result_is_ok);
    }

    for(auto index = 0U; index < 4U; ++index)
    {
      static_cast<void>(index);

      const auto flt_near_one = dist(gen);

      const auto flt_lt1 = static_cast<float_type>(1.0L) / flt_near_one;
      const auto flt_gt1 = static_cast<float_type>(1.0L) * flt_near_one;

      const auto dec_neg_lt1 = pow(static_cast<decimal_type>(flt_gt1), -std::numeric_limits<decimal_type>::infinity());
      const auto dec_pos_lt1 = pow(static_cast<decimal_type>(flt_lt1),  std::numeric_limits<decimal_type>::infinity());
      const auto dec_neg_gt1 = pow(static_cast<decimal_type>(flt_gt1), -std::numeric_limits<decimal_type>::infinity());
      const auto dec_pos_gt1 = pow(static_cast<decimal_type>(flt_lt1),  std::numeric_limits<decimal_type>::infinity());

      const auto flt_neg_lt1 = pow(flt_gt1, -std::numeric_limits<float_type>::infinity());
      const auto flt_pos_lt1 = pow(flt_lt1,  std::numeric_limits<float_type>::infinity());
      const auto flt_neg_gt1 = pow(flt_gt1, -std::numeric_limits<float_type>::infinity());
      const auto flt_pos_gt1 = pow(flt_lt1,  std::numeric_limits<float_type>::infinity());

      const auto result_pow_is_ok =
        (
             (isinf(dec_neg_lt1) ==                          isinf(flt_neg_lt1))
          && (     (dec_pos_lt1  == ::my_zero<decimal_type>()) == (flt_pos_lt1 == static_cast<float_type>(0.0L)))
          && (     (dec_neg_gt1  == ::my_zero<decimal_type>()) == (flt_neg_gt1 == static_cast<float_type>(0.0L)))
          && (isinf(dec_pos_gt1) ==                          isinf(flt_pos_gt1))
        );

      BOOST_TEST(result_pow_is_ok);

      result_is_ok = (result_pow_is_ok && result_is_ok);
    }

    for(auto index = 0U; index < 4U; ++index)
    {
      static_cast<void>(index);

      const auto flt_a = dist_a(gen);

      const auto dec_neg_neg = pow(-std::numeric_limits<decimal_type>::infinity() * static_cast<decimal_type>(dist(gen)), -static_cast<decimal_type>(flt_a));
      const auto dec_neg_pos = pow(-std::numeric_limits<decimal_type>::infinity() * static_cast<decimal_type>(dist(gen)),  static_cast<decimal_type>(flt_a));
      const auto dec_pos_neg = pow( std::numeric_limits<decimal_type>::infinity() * static_cast<decimal_type>(dist(gen)), -static_cast<decimal_type>(flt_a));
      const auto dec_pos_pos = pow( std::numeric_limits<decimal_type>::infinity() * static_cast<decimal_type>(dist(gen)),  static_cast<decimal_type>(flt_a));

      const auto flt_neg_neg = pow(-std::numeric_limits<float_type>::infinity(), -flt_a);
      const auto flt_neg_pos = pow(-std::numeric_limits<float_type>::infinity(),  flt_a);
      const auto flt_pos_neg = pow( std::numeric_limits<float_type>::infinity(), -flt_a);
      const auto flt_pos_pos = pow( std::numeric_limits<float_type>::infinity(),  flt_a);

      const auto result_pow_is_ok =
        (
             (     (dec_neg_neg == ::my_zero<decimal_type>()) ==      (flt_neg_neg == static_cast<float_type>(0.0L)))
          && (isinf(dec_neg_pos)                              == isinf(flt_neg_pos))
          && (     (dec_pos_neg == ::my_zero<decimal_type>()) ==      (flt_pos_neg == static_cast<float_type>(0.0L)))
          && (isinf(dec_pos_pos)                              == isinf(flt_pos_pos))
        );

      BOOST_TEST(result_pow_is_ok);

      result_is_ok = (result_pow_is_ok && result_is_ok);
    }

    for(auto index = 0U; index < 4U; ++index)
    {
      static_cast<void>(index);

      const auto dec_neg_neg = pow(-std::numeric_limits<decimal_type>::infinity(), -std::numeric_limits<decimal_type>::infinity());
      const auto dec_neg_pos = pow(-std::numeric_limits<decimal_type>::infinity(),  std::numeric_limits<decimal_type>::infinity());
      const auto dec_pos_neg = pow( std::numeric_limits<decimal_type>::infinity(), -std::numeric_limits<decimal_type>::infinity());
      const auto dec_pos_pos = pow( std::numeric_limits<decimal_type>::infinity(),  std::numeric_limits<decimal_type>::infinity());

      const auto flt_neg_neg = pow(-std::numeric_limits<float_type>::infinity(), -std::numeric_limits<float_type>::infinity());
      const auto flt_neg_pos = pow(-std::numeric_limits<float_type>::infinity(),  std::numeric_limits<float_type>::infinity());
      const auto flt_pos_neg = pow( std::numeric_limits<float_type>::infinity(), -std::numeric_limits<float_type>::infinity());
      const auto flt_pos_pos = pow( std::numeric_limits<float_type>::infinity(),  std::numeric_limits<float_type>::infinity());

      const auto result_pow_is_ok =
        (
             (     (dec_neg_neg == ::my_zero<decimal_type>()) ==      (flt_neg_neg == static_cast<float_type>(0.0L)))
          && (isinf(dec_neg_pos)                              == isinf(flt_neg_pos))
          && (     (dec_pos_neg == ::my_zero<decimal_type>()) ==      (flt_pos_neg == static_cast<float_type>(0.0L)))
          && (isinf(dec_pos_pos)                              == isinf(flt_pos_pos))
        );

      BOOST_TEST(result_pow_is_ok);

      result_is_ok = (result_pow_is_ok && result_is_ok);
    }

    for(auto index = 0U; index < 8U; ++index)
    {
      static_cast<void>(index);

      const auto flt_a = dist_a(gen);

      const auto dec_neg_nan = pow(-std::numeric_limits<decimal_type>::infinity(), ::my_nan<decimal_type>() * static_cast<decimal_type>(dist(gen)));
      const auto dec_pos_nan = pow( std::numeric_limits<decimal_type>::infinity(), ::my_nan<decimal_type>() * static_cast<decimal_type>(dist(gen)));
      const auto dec_nan_neg = pow( ::my_nan<decimal_type>() * static_cast<decimal_type>(dist(gen)), -static_cast<decimal_type>(flt_a));
      const auto dec_nan_pos = pow( ::my_nan<decimal_type>() * static_cast<decimal_type>(dist(gen)),  static_cast<decimal_type>(flt_a));

      const auto flt_neg_nan = pow(-std::numeric_limits<float_type>::infinity(), std::numeric_limits<float_type>::quiet_NaN());
      const auto flt_pos_nan = pow( std::numeric_limits<float_type>::infinity(), std::numeric_limits<float_type>::quiet_NaN());
      const auto flt_nan_neg = pow( std::numeric_limits<float_type>::quiet_NaN(), -flt_a);
      const auto flt_nan_pos = pow( std::numeric_limits<float_type>::quiet_NaN(),  flt_a);

      using std::isnan;

      const auto result_pow_is_ok =
        (
             (isnan(dec_neg_nan) == isnan(flt_neg_nan))
          && (isnan(dec_pos_nan) == isnan(flt_pos_nan))
          && (isnan(dec_nan_neg) == isnan(flt_nan_neg))
          && (isnan(dec_nan_pos) == isnan(flt_nan_pos))
        );

      BOOST_TEST(result_pow_is_ok);

      result_is_ok = (result_pow_is_ok && result_is_ok);
    }

    for(auto index = 0U; index < 4U; ++index)
    {
      static_cast<void>(index);

      const auto flt_near_one = dist(gen);

      const auto dec_neg_neg = pow(-one, -std::numeric_limits<decimal_type>::infinity() * static_cast<decimal_type>(flt_near_one));
      const auto dec_neg_pos = pow(-one,  std::numeric_limits<decimal_type>::infinity() * static_cast<decimal_type>(flt_near_one));
      const auto dec_pos_neg = pow( one, -std::numeric_limits<decimal_type>::infinity() * static_cast<decimal_type>(flt_near_one));
      const auto dec_pos_pos = pow( one,  std::numeric_limits<decimal_type>::infinity() * static_cast<decimal_type>(flt_near_one));

      const auto flt_neg_neg = pow(-static_cast<float_type>(1.0L), -std::numeric_limits<float_type>::infinity() * flt_near_one);
      const auto flt_neg_pos = pow(-static_cast<float_type>(1.0L),  std::numeric_limits<float_type>::infinity() * flt_near_one);
      const auto flt_pos_neg = pow( static_cast<float_type>(1.0L), -std::numeric_limits<float_type>::infinity() * flt_near_one);
      const auto flt_pos_pos = pow( static_cast<float_type>(1.0L),  std::numeric_limits<float_type>::infinity() * flt_near_one);

      const auto result_pow_is_ok =
        (
             ((dec_neg_neg == ::my_one<decimal_type>()) == (flt_neg_neg == static_cast<float_type>(1.0L)))
          && ((dec_neg_pos == ::my_one<decimal_type>()) == (flt_neg_pos == static_cast<float_type>(1.0L)))
          && ((dec_pos_neg == ::my_one<decimal_type>()) == (flt_pos_neg == static_cast<float_type>(1.0L)))
          && ((dec_pos_pos == ::my_one<decimal_type>()) == (flt_pos_pos == static_cast<float_type>(1.0L)))
        );

      BOOST_TEST(result_pow_is_ok);

      result_is_ok = (result_pow_is_ok && result_is_ok);
    }

    return result_is_ok;
  }

  template<typename DecimalType, typename FloatType>
  auto test_pow_n_edge(const int tol_factor) -> bool
  {
    static_cast<void>(tol_factor);

    using decimal_type = DecimalType;
    using float_type   = FloatType;

    std::mt19937_64 gen;

    gen.seed(time_point<typename std::mt19937_64::result_type>());

    std::uniform_real_distribution<float_type>
      dist
      (
        static_cast<float_type>(1.01L),
        static_cast<float_type>(1.04L)
      );

    std::uniform_int_distribution<int>
      dist_n
      (
        2,
        12
      );

    using std::fpclassify;
    using std::isinf;
    using std::pow;
    using std::signbit;

    auto result_is_ok = true;

    for(auto index = 0U; index < 8U; ++index)
    {
      static_cast<void>(index);

      const float_type   flt_x = dist  (gen);
      const decimal_type dec_x = static_cast<decimal_type>(flt_x);

      using std::pow;

      const auto dec_nrm = pow(dec_x, 0);
      const auto flt_nrm = pow(flt_x, 0);

      const auto result_val_pow_zero_is_ok = ((dec_nrm == decimal_type { 1 }) && (flt_nrm == static_cast<float_type>(1.0L)));

      BOOST_TEST(result_val_pow_zero_is_ok);

      result_is_ok = (result_val_pow_zero_is_ok && result_is_ok);
    }

    for(auto index = 0U; index < 8U; ++index)
    {
      static_cast<void>(index);

      const decimal_type arg_nan = ::my_nan<decimal_type>() * static_cast<decimal_type>(dist(gen));

      const auto dec_pow_nan = pow(arg_nan, dist_n(gen));

      const auto result_val_pow_nan_is_ok = (isnan(dec_pow_nan) && isnan(arg_nan));

      BOOST_TEST(result_val_pow_nan_is_ok);

      result_is_ok = (result_val_pow_nan_is_ok && result_is_ok);
    }

    for(auto index = 0U; index < 8U; ++index)
    {
      static_cast<void>(index);

      const decimal_type arg_x_nrm  = static_cast<decimal_type>(dist(gen));
      const decimal_type arg_p_zero = static_cast<decimal_type>(dist_n(gen)) * (::my_zero<decimal_type>() * static_cast<decimal_type>(dist(gen)));
      const int          n_p_zero   = static_cast<int>(arg_p_zero);

      const auto dec_pow_zero = pow(arg_x_nrm, n_p_zero);

      const auto result_val_pow_zero_is_ok = ((dec_pow_zero == decimal_type { 1 }) && (n_p_zero == 0));

      BOOST_TEST(result_val_pow_zero_is_ok);

      result_is_ok = (result_val_pow_zero_is_ok && result_is_ok);
    }

    for(auto index = 0U; index < 8U; ++index)
    {
      static_cast<void>(index);

      const decimal_type arg_p_zero = static_cast<decimal_type>(dist_n(gen)) * (::my_zero<decimal_type>() * static_cast<decimal_type>(dist(gen)));
      const int          n_p_zero   = static_cast<int>(arg_p_zero);

      const auto dec_pow2_zero = boost::decimal::detail::pow_2_impl<decimal_type>(n_p_zero);

      const auto result_val_pow2_zero_is_ok = ((dec_pow2_zero == decimal_type { 1 }) && (n_p_zero == 0));

      BOOST_TEST(result_val_pow2_zero_is_ok);

      result_is_ok = (result_val_pow2_zero_is_ok && result_is_ok);
    }

    for(auto index = 2; index <= 10; index += 2)
    {
      const auto dec_zero_pos = pow(::my_zero<decimal_type>() * static_cast<decimal_type>(dist(gen)), decimal_type(index));
      const auto flt_zero_pos = pow(static_cast<float_type>(0.0L), static_cast<float_type>(index));

      const auto result_val_zero_pos_is_ok =
        (
             ((fpclassify(dec_zero_pos) == FP_ZERO) == (fpclassify(flt_zero_pos) == FP_ZERO))
          && (signbit(dec_zero_pos) == signbit(flt_zero_pos))
        );

      BOOST_TEST(result_val_zero_pos_is_ok);

      result_is_ok = (result_val_zero_pos_is_ok && result_is_ok);
    }

    for(auto index = 2; index <= 10; index += 2)
    {
      const auto dec_zero_neg = pow(-::my_zero<decimal_type>() * static_cast<decimal_type>(dist(gen)), decimal_type(index));
      const auto flt_zero_neg = pow(static_cast<float_type>(-0.0L), static_cast<float_type>(index));

      const auto result_val_zero_neg_is_ok =
        (
             ((fpclassify(dec_zero_neg) == FP_ZERO) == (fpclassify(flt_zero_neg) == FP_ZERO))
          && (signbit(dec_zero_neg) == signbit(flt_zero_neg))
        );

      BOOST_TEST(result_val_zero_neg_is_ok);

      result_is_ok = (result_val_zero_neg_is_ok && result_is_ok);
    }

    for(auto index = 3; index <= 11; index += 2)
    {
      const auto dec_zero_pos = pow(::my_zero<decimal_type>() * static_cast<decimal_type>(dist(gen)), decimal_type(index));
      const auto flt_zero_pos = pow(static_cast<float_type>(0.0L), static_cast<float_type>(index));

      const auto result_val_zero_pos_is_ok =
        (
             ((fpclassify(dec_zero_pos) == FP_ZERO) == (fpclassify(flt_zero_pos) == FP_ZERO))
          && (signbit(dec_zero_pos) == signbit(flt_zero_pos))
        );

      BOOST_TEST(result_val_zero_pos_is_ok);

      result_is_ok = (result_val_zero_pos_is_ok && result_is_ok);
    }

    for(auto index = 3; index <= 11; index += 2)
    {
      const decimal_type arg_zero_neg = -(::my_zero<decimal_type>() * static_cast<decimal_type>(dist(gen)));

      const auto dec_zero_neg = pow(arg_zero_neg, decimal_type(index));
      const auto flt_zero_neg = pow(static_cast<float_type>(-0.0L), static_cast<float_type>(index));

      const auto result_val_zero_neg_is_ok =
        (
             ((fpclassify(dec_zero_neg) == FP_ZERO) == (fpclassify(flt_zero_neg) == FP_ZERO))
          && (signbit(dec_zero_neg) == signbit(flt_zero_neg))
        );

      BOOST_TEST(result_val_zero_neg_is_ok);

      result_is_ok = (result_val_zero_neg_is_ok && result_is_ok);
    }

    for(auto index = -11; index <= -3; index += 2)
    {
      const auto dec_zero_pos = pow(::my_zero<decimal_type>() * static_cast<decimal_type>(dist(gen)), decimal_type(index));
      const auto flt_zero_pos = pow(static_cast<float_type>(0.0L), static_cast<float_type>(index));

      const auto result_val_zero_pos_is_ok =
        (
             (isinf  (dec_zero_pos) == isinf  (flt_zero_pos))
          && (signbit(dec_zero_pos) == signbit(flt_zero_pos))
        );

      BOOST_TEST(result_val_zero_pos_is_ok);

      result_is_ok = (result_val_zero_pos_is_ok && result_is_ok);
    }

    for(auto index = -11; index <= -3; index += 2)
    {
      const auto dec_zero_neg = pow(-::my_zero<decimal_type>() * static_cast<decimal_type>(dist(gen)), decimal_type(index));

      const auto result_val_zero_neg_is_ok = (isinf(dec_zero_neg) && (!signbit(dec_zero_neg)));

      BOOST_TEST(result_val_zero_neg_is_ok);

      result_is_ok = (result_val_zero_neg_is_ok && result_is_ok);
    }

    for(auto index = -10; index <= -2; index += 2)
    {
      const auto dec_zero_pos = pow(::my_zero<decimal_type>() * static_cast<decimal_type>(dist(gen)), decimal_type(index));
      const auto flt_zero_pos = pow(static_cast<float_type>(0.0L), static_cast<float_type>(index));

      const auto result_val_zero_pos_is_ok =
        (
             (isinf  (dec_zero_pos) == isinf  (flt_zero_pos))
          && (signbit(dec_zero_pos) == signbit(flt_zero_pos))
        );

      BOOST_TEST(result_val_zero_pos_is_ok);

      result_is_ok = (result_val_zero_pos_is_ok && result_is_ok);
    }

    for(auto index = -10; index <= -2; index += 2)
    {
      const auto dec_zero_neg = pow(-::my_zero<decimal_type>() * static_cast<decimal_type>(dist(gen)), decimal_type(index));
      const auto flt_zero_neg = pow(-static_cast<float_type>(0.0L), static_cast<float_type>(index));

      const auto result_val_zero_neg_is_ok =
        (
             (isinf  (dec_zero_neg) == isinf  (flt_zero_neg))
          && (signbit(dec_zero_neg) == signbit(flt_zero_neg))
        );

      BOOST_TEST(result_val_zero_neg_is_ok);

      result_is_ok = (result_val_zero_neg_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(8)); ++i)
    {
      static_cast<void>(i);

      const auto flt_near_one = dist(gen);

      const auto dec_inf_neg = pow(-std::numeric_limits<decimal_type>::infinity() * static_cast<decimal_type>(flt_near_one), -3);
      const auto flt_inf_neg = pow(-std::numeric_limits<float_type  >::infinity() * flt_near_one, -3);

      const auto result_val_inf_neg_is_ok =
        (
             ((fpclassify(dec_inf_neg) == FP_ZERO) == (fpclassify(flt_inf_neg) == FP_ZERO))
          && (signbit(dec_inf_neg) == signbit(flt_inf_neg))
        );

      BOOST_TEST(result_val_inf_neg_is_ok);

      result_is_ok = (result_val_inf_neg_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(8)); ++i)
    {
      static_cast<void>(i);

      const auto flt_near_one = dist(gen);

      const auto dec_inf_neg = pow(-std::numeric_limits<decimal_type>::infinity() * static_cast<decimal_type>(flt_near_one), 3);
      const auto flt_inf_neg = pow(-std::numeric_limits<float_type  >::infinity() * flt_near_one, 3);

      const auto result_val_inf_neg_is_ok =
        (
             ((fpclassify(dec_inf_neg) == FP_INFINITE) == (fpclassify(flt_inf_neg) == FP_INFINITE))
          && (signbit(dec_inf_neg) == signbit(flt_inf_neg))
        );

      BOOST_TEST(result_val_inf_neg_is_ok);

      result_is_ok = (result_val_inf_neg_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(8)); ++i)
    {
      static_cast<void>(i);

      const auto flt_near_one = dist(gen);

      const auto dec_inf_pos = pow(std::numeric_limits<decimal_type>::infinity() * static_cast<decimal_type>(flt_near_one), -3);
      const auto flt_inf_pos = pow(std::numeric_limits<float_type  >::infinity() * flt_near_one, -3);

      const auto result_val_inf_pos_is_ok =
        (
             ((fpclassify(dec_inf_pos) == FP_ZERO) == (fpclassify(flt_inf_pos) == FP_ZERO))
          && (signbit(dec_inf_pos) == signbit(flt_inf_pos))
        );

      BOOST_TEST(result_val_inf_pos_is_ok);

      result_is_ok = (result_val_inf_pos_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(8)); ++i)
    {
      static_cast<void>(i);

      const auto flt_near_one = dist(gen);

      const auto dec_inf_pos = pow(std::numeric_limits<decimal_type>::infinity() * static_cast<decimal_type>(flt_near_one), 3);
      const auto flt_inf_pos = pow(std::numeric_limits<float_type  >::infinity() * flt_near_one, 3);

      const auto result_val_inf_pos_is_ok =
        (
             ((fpclassify(dec_inf_pos) == FP_INFINITE) == (fpclassify(flt_inf_pos) == FP_INFINITE))
          && (signbit(dec_inf_pos) == signbit(flt_inf_pos))
        );

      BOOST_TEST(result_val_inf_pos_is_ok);

      result_is_ok = (result_val_inf_pos_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(8)); ++i)
    {
      static_cast<void>(i);

      const auto flt_near_one = dist(gen);

      const auto dec_inf_pos = pow(std::numeric_limits<decimal_type>::infinity() * static_cast<decimal_type>(flt_near_one), 0);
      const auto flt_inf_pos = pow(std::numeric_limits<float_type  >::infinity() * flt_near_one, 0);

      const auto result_val_inf_pos_is_ok = ((dec_inf_pos == decimal_type { 1, 0 }) == (flt_inf_pos == static_cast<float_type>(1.0L)));

      BOOST_TEST(result_val_inf_pos_is_ok);

      result_is_ok = (result_val_inf_pos_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(8)); ++i)
    {
      static_cast<void>(i);

      const auto flt_near_one = dist(gen);

      const auto dec_inf_neg = pow(-std::numeric_limits<decimal_type>::infinity() * static_cast<decimal_type>(flt_near_one), 0);
      const auto flt_inf_neg = pow(-std::numeric_limits<float_type  >::infinity() * flt_near_one, 0);

      const auto result_val_inf_neg_is_ok = ((dec_inf_neg == decimal_type { 1, 0 }) == (flt_inf_neg == static_cast<float_type>(1.0L)));

      BOOST_TEST(result_val_inf_neg_is_ok);

      result_is_ok = (result_val_inf_neg_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(8)); ++i)
    {
      static_cast<void>(i);

      const auto flt_near_n = static_cast<float_type>(dist(gen) + static_cast<float_type>(i));

      const auto dec_near_n_pi = pow(boost::decimal::numbers::pi_v<decimal_type> * static_cast<decimal_type>(flt_near_n), 0);

      const auto result_val_near_pi_is_ok = (dec_near_n_pi == decimal_type { 1, 0 });

      BOOST_TEST(result_val_near_pi_is_ok);

      result_is_ok = (result_val_near_pi_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(8)); ++i)
    {
      static_cast<void>(i);

      const auto flt_near_one = dist(gen);

      const auto dec_nan = pow(std::numeric_limits<decimal_type>::quiet_NaN() * static_cast<decimal_type>(flt_near_one), 0);

      const auto result_val_nan_is_ok = (dec_nan == decimal_type { 1, 0 });

      BOOST_TEST(result_val_nan_is_ok);

      result_is_ok = (result_val_nan_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(8)); ++i)
    {
      static_cast<void>(i);

      const auto flt_near_one = dist(gen);

      const auto dec_nan = pow(std::numeric_limits<decimal_type>::quiet_NaN() * static_cast<decimal_type>(flt_near_one), i + 1);

      const auto result_val_nan_is_ok = isnan(dec_nan);

      BOOST_TEST(result_val_nan_is_ok);

      result_is_ok = (result_val_nan_is_ok && result_is_ok);
    }

    return result_is_ok;
  }
}

auto main() -> int
{
  auto result_is_ok = true;

  {
    using decimal_type = boost::decimal::decimal32_t;
    using float_type   = float;

    const auto test_pow_edge_is_ok   = local::test_pow_edge  <decimal_type, float_type>();
    const auto test_pow_n_edge_is_ok = local::test_pow_n_edge<decimal_type, float_type>(512);
    const auto test_pow_pos_is_ok    = local::test_pow       <decimal_type, float_type>(512, false);
    const auto test_pow_neg_is_ok    = local::test_pow       <decimal_type, float_type>(512, true);
    const auto test_pow_n_is_ok      = local::test_pow_n     <decimal_type, float_type>(512);

    const auto result_test_pow_is_ok = (test_pow_pos_is_ok && test_pow_neg_is_ok && test_pow_edge_is_ok && test_pow_n_edge_is_ok && test_pow_n_is_ok);

    result_is_ok = (result_test_pow_is_ok && result_is_ok);
  }

  {
    using decimal_type = boost::decimal::decimal32_t;

    const auto test_10_pow_n_is_ok   = local::test_10_pow_n<decimal_type>();

    result_is_ok = (test_10_pow_n_is_ok && result_is_ok);
  }

  {
    using decimal_type = boost::decimal::decimal64_t;
    using float_type   = double;

    const auto test_pow_edge_is_ok   = local::test_pow_edge  <decimal_type, float_type>();
    const auto test_pow_n_edge_is_ok = local::test_pow_n_edge<decimal_type, float_type>(1024);
    const auto test_pow_pos_is_ok    = local::test_pow<decimal_type, float_type>(1024, false);
    const auto test_pow_is_neg_ok    = local::test_pow<decimal_type, float_type>(1024, true);

    result_is_ok = (test_pow_edge_is_ok && test_pow_n_edge_is_ok && test_pow_pos_is_ok && test_pow_is_neg_ok && result_is_ok);
  }

  result_is_ok = ((boost::report_errors() == 0) && result_is_ok);

  return (result_is_ok ? 0 : -1);
}

template<typename DecimalType> auto my_zero() -> DecimalType& { using decimal_type = DecimalType; static decimal_type my_val_zero { 0, 0 }; return my_val_zero; }
template<typename DecimalType> auto my_one () -> DecimalType& { using decimal_type = DecimalType; static decimal_type my_val_one  { 1, 0 }; return my_val_one; }
template<typename DecimalType> auto my_ten () -> DecimalType& { using decimal_type = DecimalType; static decimal_type my_val_ten  { 1, 1 }; return my_val_ten; }
template<typename DecimalType> auto my_inf () -> DecimalType& { using decimal_type = DecimalType; static decimal_type my_val_inf  { std::numeric_limits<decimal_type>::infinity() };  return my_val_inf; }
template<typename DecimalType> auto my_nan () -> DecimalType& { using decimal_type = DecimalType; static decimal_type my_val_nan  { std::numeric_limits<decimal_type>::quiet_NaN() }; return my_val_nan; }
