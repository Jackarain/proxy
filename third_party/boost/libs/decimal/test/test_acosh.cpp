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
auto my_one () -> boost::decimal::decimal32_t&;

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

  auto test_acosh(const std::int32_t tol_factor, const long double range_lo, const long double range_hi) -> bool
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
      const auto x_flt = dis(gen);
      const auto x_dec = static_cast<decimal_type>(x_flt);

      using std::acosh;

      const auto val_flt = acosh(x_flt);
      const auto val_dec = acosh(x_dec);

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

  auto test_acosh_edge() -> bool
  {
    using decimal_type = boost::decimal::decimal32_t;

    std::mt19937_64 gen;

    std::uniform_real_distribution<float> dist(1.01F, 1.04F);

    auto result_is_ok = true;

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_nan_pos = acosh(std::numeric_limits<decimal_type>::quiet_NaN() * static_cast<decimal_type>(dist(gen)));

      const auto result_val_nan_pos_is_ok = isnan(val_nan_pos) && (!signbit(val_nan_pos));

      BOOST_TEST(result_val_nan_pos_is_ok);

      result_is_ok = (result_val_nan_pos_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_nan_neg = acosh(-std::numeric_limits<decimal_type>::quiet_NaN() * static_cast<decimal_type>(dist(gen)));

      const auto result_val_nan_neg_is_ok = isnan(val_nan_neg) && (signbit(val_nan_neg));

      BOOST_TEST(result_val_nan_neg_is_ok);

      result_is_ok = (result_val_nan_neg_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_inf_pos = acosh(std::numeric_limits<decimal_type>::infinity() * static_cast<decimal_type>(dist(gen)));

      const auto result_val_inf_pos_is_ok = (isinf(val_inf_pos) && (!signbit(val_inf_pos)));

      BOOST_TEST(result_val_inf_pos_is_ok);

      result_is_ok = (result_val_inf_pos_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_one = acosh(::my_one());

      const auto result_val_one_is_ok = (val_one == ::my_zero());

      BOOST_TEST(result_val_one_is_ok);

      result_is_ok = (result_val_one_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_lt_one = acosh(::my_one() / static_cast<decimal_type>(dist(gen) * (dist(gen) * dist(gen))));

      const auto result_val_lt_one_is_ok = (isnan(val_lt_one) && signbit(val_lt_one));

      BOOST_TEST(result_val_lt_one_is_ok);

      result_is_ok = (result_val_lt_one_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_zero_pos = acosh(::my_zero());

      const auto result_val_zero_pos_is_ok = (isnan(val_zero_pos) && (signbit(val_zero_pos)));

      BOOST_TEST(result_val_zero_pos_is_ok);

      result_is_ok = (result_val_zero_pos_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_zero_neg = acosh(-::my_zero());

      const auto result_val_zero_neg_is_ok = (isnan(val_zero_neg) && (signbit(val_zero_neg)));

      BOOST_TEST(result_val_zero_neg_is_ok);

      result_is_ok = (result_val_zero_neg_is_ok && result_is_ok);
    }

    return result_is_ok;
  }

} // namespace local

auto main() -> int
{
  auto result_is_ok = true;

  const auto result_eps_is_ok =
    local::test_acosh
    (
      INT32_C(16) * INT32_C(262144),
      1.0L + static_cast<double>(std::numeric_limits<boost::decimal::decimal32_t>::epsilon()) * 10.0L,
      1.0L + static_cast<double>(std::numeric_limits<boost::decimal::decimal32_t>::epsilon()) * 100.0L
    );

  const auto result_tiny_is_ok   = local::test_acosh(INT32_C(4096), 1.001L, 1.1L);
  const auto result_small_is_ok  = local::test_acosh(INT32_C(96), 1.1L, 1.59L);
  const auto result_medium_is_ok = local::test_acosh(INT32_C(48), 1.59L, 10.1L);
  const auto result_large_is_ok  = local::test_acosh(INT32_C(48), 1.0E+01L, 1.0E+26L);

  BOOST_TEST(result_eps_is_ok);
  BOOST_TEST(result_tiny_is_ok);
  BOOST_TEST(result_small_is_ok);
  BOOST_TEST(result_medium_is_ok);
  BOOST_TEST(result_large_is_ok);

  const auto result_edge_is_ok = local::test_acosh_edge();

  const auto result_ranges_is_ok = (result_eps_is_ok && result_tiny_is_ok && result_small_is_ok && result_medium_is_ok && result_large_is_ok);

  result_is_ok = (result_ranges_is_ok && result_is_ok);

  BOOST_TEST(result_edge_is_ok);

  result_is_ok = (result_edge_is_ok && result_is_ok);

  result_is_ok = ((boost::report_errors() == 0) && result_is_ok);

  return (result_is_ok ? 0 : -1);
}

auto my_zero() -> boost::decimal::decimal32_t& { static boost::decimal::decimal32_t val_zero { 0, 0 }; return val_zero; }
auto my_one () -> boost::decimal::decimal32_t& { static boost::decimal::decimal32_t val_one  { 1, 0 }; return val_one; }
