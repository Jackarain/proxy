// Copyright 2023 Matt Borland
// Copyright 2023 Christopher Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include "testing_config.hpp"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>

#include <boost/decimal.hpp>

#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wfloat-equal"
#elif defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#endif

#include <boost/core/lightweight_test.hpp>

auto my_zero() -> boost::decimal::decimal32_t&;

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

  auto test_tan(const int tol_factor, const bool negate, const long double range_lo, const long double range_hi) -> bool
  {
    using decimal_type = boost::decimal::decimal32_t;

    std::random_device rd;
    std::mt19937_64 gen(rd());

    gen.seed(time_point<typename std::mt19937_64::result_type>());

    auto dis =
      std::uniform_real_distribution<float>
      {
        static_cast<float>(range_lo),
        static_cast<float>(range_hi)
      };

    auto result_is_ok = true;

    auto trials = static_cast<std::uint32_t>(UINT8_C(0));

    #if !defined(BOOST_DECIMAL_REDUCE_TEST_DEPTH)
    constexpr auto count = UINT32_C(0x400);
    #else
    constexpr auto count = UINT32_C(0x40);
    #endif

    for( ; trials < count; ++trials)
    {
      const auto x_flt_begin = dis(gen);

      const auto x_flt = (negate ? -x_flt_begin : x_flt_begin);
      const auto x_dec = static_cast<decimal_type>(x_flt);

      using std::tan;

      const auto val_flt = tan(x_flt);
      const auto val_dec = tan(x_dec);

      const auto result_val_is_ok = is_close_fraction(val_flt, static_cast<float>(val_dec), std::numeric_limits<float>::epsilon() * static_cast<float>(tol_factor));

      result_is_ok = (result_val_is_ok && result_is_ok);

      if(!result_val_is_ok)
      {
          // LCOV_EXCL_START
        std::cout << "x_flt  : " <<                    x_flt   << std::endl;
        std::cout << "val_flt: " << std::scientific << val_flt << std::endl;
        std::cout << "val_dec: " << std::scientific << val_dec << std::endl;

        break;
          // LCOV_EXCL_STOP
      }
    }

    BOOST_TEST(result_is_ok);

    return result_is_ok;
  }

  auto test_tan_phase() -> bool
  {
    // Check the phases of large positive/negative arguments.
    using std::atan;

    auto result_is_ok = true;

    for(auto x = 0.1F; x < 20.0F; x += 2.0F * atan(1.0F))
    {
      using std::tan;

      const auto result_signs_are_same_is_ok = ((tan(boost::decimal::decimal32_t { x }) < 0) == (tan(x) < 0));

      BOOST_TEST(result_signs_are_same_is_ok);

      result_is_ok = (result_signs_are_same_is_ok && result_signs_are_same_is_ok);
    }

    for(auto x = 0.1F; x < 20.0F; x += 2.0F * atan(1.0F))
    {
      using std::tan;

      const auto result_signs_are_same_is_ok = ((tan(boost::decimal::decimal32_t { -x }) < 0) == (tan(-x) < 0));

      BOOST_TEST(result_signs_are_same_is_ok);

      result_is_ok = (result_signs_are_same_is_ok && result_signs_are_same_is_ok);
    }

    return result_is_ok;
  }

  auto test_tan_edge() -> bool
  {
    using decimal_type = boost::decimal::decimal32_t;

    std::mt19937_64 gen;

    std::uniform_real_distribution<float> dist(1.01F, 1.04F);

    auto result_is_ok = true;

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_nan = tan(std::numeric_limits<decimal_type>::quiet_NaN() * static_cast<decimal_type>(dist(gen)));

      const auto result_val_nan_is_ok = isnan(val_nan);

      BOOST_TEST(result_val_nan_is_ok);

      result_is_ok = (result_val_nan_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_inf_pos = tan(std::numeric_limits<decimal_type>::infinity() * static_cast<decimal_type>(dist(gen)));

      const auto result_val_inf_pos_is_ok = isnan(val_inf_pos);

      BOOST_TEST(result_val_inf_pos_is_ok);

      result_is_ok = (result_val_inf_pos_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_inf_neg = tan(-std::numeric_limits<decimal_type>::infinity() * static_cast<decimal_type>(dist(gen)));

      const auto result_val_inf_neg_is_ok = isnan(val_inf_neg);

      BOOST_TEST(result_val_inf_neg_is_ok);

      result_is_ok = (result_val_inf_neg_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_zero_pos = tan(::my_zero());

      const auto result_val_zero_pos_is_ok = (val_zero_pos == ::my_zero());

      BOOST_TEST(result_val_zero_pos_is_ok);

      result_is_ok = (result_val_zero_pos_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_zero_neg = tan(-::my_zero());

      const auto result_val_zero_neg_is_ok = (-val_zero_neg == ::my_zero());

      BOOST_TEST(result_val_zero_neg_is_ok);

      result_is_ok = (result_val_zero_neg_is_ok && result_is_ok);
    }

    return result_is_ok;
  }
} // namespace local

auto main() -> int
{
  auto result_is_ok = true;

  const auto result_pos_is_ok              = local::test_tan(256,  false, 0.03125L, 1.48L);
  const auto result_neg_is_ok              = local::test_tan(256,  true,  0.03125L, 1.48L);
  const auto result_pos_near_pi_half_is_ok = local::test_tan(4096, false, 1.48L,    1.56L);

  const auto result_phase_is_ok            = local::test_tan_phase();

  const auto result_edge_is_ok = local::test_tan_edge();

  BOOST_TEST(result_pos_is_ok);
  BOOST_TEST(result_neg_is_ok);
  BOOST_TEST(result_pos_near_pi_half_is_ok);
  BOOST_TEST(result_phase_is_ok);
  BOOST_TEST(result_edge_is_ok);

  result_is_ok = (result_pos_is_ok              && result_is_ok);
  result_is_ok = (result_neg_is_ok              && result_is_ok);
  result_is_ok = (result_pos_near_pi_half_is_ok && result_is_ok);
  result_is_ok = (result_phase_is_ok            && result_is_ok);
  result_is_ok = (result_edge_is_ok             && result_is_ok);

  result_is_ok = ((boost::report_errors() == 0) && result_is_ok);

  return (result_is_ok ? 0 : -1);
}

auto my_zero() -> boost::decimal::decimal32_t& { static boost::decimal::decimal32_t val_zero { 0, 0 }; return val_zero; }
