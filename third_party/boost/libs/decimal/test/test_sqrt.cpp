// Copyright 2024 - 2026 Matt Borland
// Copyright 2024 - 2026 Christopher Kormanyos
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

template<typename DecimalType> auto my_zero() -> DecimalType& { using decimal_type = DecimalType; static decimal_type val_zero { 0, 0 }; return val_zero; }
template<typename DecimalType> auto my_one () -> DecimalType& { using decimal_type = DecimalType; static decimal_type val_one  { 1, 0 }; return val_one; }

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

    const NumericType delta
    {
      (b == static_cast<NumericType>(0)) ? fabs(a - b) : fabs(1 - (a / b))
    };

    const bool result_is_ok { (delta < tol) };

    // LCOV_EXCL_START
    if (!result_is_ok)
    {
      std::cerr << std::setprecision(std::numeric_limits<NumericType>::digits10) << "a: " << a
                << "\nb: " << b
                << "\ndelta: " << delta
                << "\ntol: " << tol << std::endl;
    }
    // LCOV_EXCL_STOP

    return result_is_ok;
  }

  template<typename DecimalType, typename FloatType>
  auto test_sqrt(const std::int32_t tol_factor, const long double range_lo, const long double range_hi) -> bool
  {
    using decimal_type = DecimalType;
    using float_type   = FloatType;

    std::random_device rd;
    std::mt19937_64 gen(rd());

    gen.seed(local::time_point<typename std::mt19937_64::result_type>());

    auto dis =
      std::uniform_real_distribution<float_type>
      {
        static_cast<float_type>(range_lo),
        static_cast<float_type>(range_hi)
      };

    auto result_is_ok = true;

    auto trials = static_cast<std::uint32_t>(UINT8_C(0));

    #if !defined(BOOST_DECIMAL_REDUCE_TEST_DEPTH)
    constexpr std::uint32_t count { ((std::numeric_limits<DecimalType>::digits10 < 10) ? UINT16_C(3200) : UINT16_C(1600)) };
    #else
    constexpr std::uint32_t count { ((std::numeric_limits<DecimalType>::digits10 < 10) ? UINT16_C(320)  : UINT16_C(160)) };
    #endif

    for( ; trials < count; ++trials)
    {
      const auto x_flt = dis(gen);
      const auto x_dec = static_cast<decimal_type>(x_flt);

      using std::sqrt;

      const auto val_flt = sqrt(x_flt);
      const auto val_dec = sqrt(x_dec);

      const auto result_val_is_ok = is_close_fraction(val_flt, static_cast<float_type>(val_dec), std::numeric_limits<float_type>::epsilon() * static_cast<float_type>(tol_factor));

      result_is_ok = (result_val_is_ok && result_is_ok);

      if(!result_val_is_ok)
      {
        // LCOV_EXCL_START
        std::cout << "x_flt  : " << std::scientific << std::setprecision(std::numeric_limits<float_type>::digits10) << x_flt   << std::endl;
        std::cout << "val_flt: " << std::scientific << std::setprecision(std::numeric_limits<float_type>::digits10) << val_flt << std::endl;
        std::cout << "val_dec: " << std::scientific << std::setprecision(std::numeric_limits<float_type>::digits10) << val_dec << std::endl;

        break;
        // LCOV_EXCL_STOP
      }
    }

    BOOST_TEST(result_is_ok);

    return result_is_ok;
  }

  template<typename DecimalType, typename FloatType>
  auto test_sqrt_edge() -> bool
  {
    using decimal_type = DecimalType;
    using float_type   = FloatType;

    std::mt19937_64 gen;

    std::uniform_real_distribution<float_type> dist(static_cast<float_type>(1.01L), static_cast<float_type>(1.04L));

    auto result_is_ok = true;

    {
      int np = -20;

      for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(41)); ++i)
      {
        static_cast<void>(i);

        const decimal_type arg_p10 { 1, np };

        const decimal_type val_p10 = sqrt(arg_p10);

        bool result_val_p10_is_ok { };

        const int np_mod2 = np % 2;

        if(np_mod2 == 0)
        {
          result_val_p10_is_ok = (val_p10 == decimal_type { 1, np / 2 });
        }
        else if(np_mod2 == -1)
        {
          decimal_type val_p10_ctrl = decimal_type { 1, np / 2 } / boost::decimal::numbers::sqrt10_v<decimal_type>;

          result_val_p10_is_ok = (val_p10 == val_p10_ctrl);
        }
        else if(np_mod2 == 1)
        {
          decimal_type val_p10_ctrl = decimal_type { 1, np / 2 } * boost::decimal::numbers::sqrt10_v<decimal_type>;

          result_val_p10_is_ok = (val_p10 == val_p10_ctrl);
        }

        ++np;

        BOOST_TEST(result_val_p10_is_ok);

        result_is_ok = (result_val_p10_is_ok && result_is_ok);
      }
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_nan_pos = sqrt(std::numeric_limits<decimal_type>::quiet_NaN() * static_cast<decimal_type>(dist(gen)));

      const auto result_val_nan_pos_is_ok = isnan(val_nan_pos) && (!signbit(val_nan_pos));

      BOOST_TEST(result_val_nan_pos_is_ok);

      result_is_ok = (result_val_nan_pos_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_nan_neg = sqrt(-std::numeric_limits<decimal_type>::quiet_NaN() * static_cast<decimal_type>(dist(gen)));

      const auto result_val_nan_neg_is_ok = isnan(val_nan_neg) && (signbit(val_nan_neg));

      BOOST_TEST(result_val_nan_neg_is_ok);

      result_is_ok = (result_val_nan_neg_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_inf_pos = sqrt(std::numeric_limits<decimal_type>::infinity() * static_cast<decimal_type>(dist(gen)));

      const auto result_val_inf_pos_is_ok = (isinf(val_inf_pos) && (!signbit(val_inf_pos)));

      BOOST_TEST(result_val_inf_pos_is_ok);

      result_is_ok = (result_val_inf_pos_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_one = sqrt(::my_one<decimal_type>());

      const auto result_val_one_is_ok = (val_one == ::my_one<decimal_type>());

      BOOST_TEST(result_val_one_is_ok);

      result_is_ok = (result_val_one_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_zero_pos = sqrt(::my_zero<decimal_type>());

      const auto result_val_zero_pos_is_ok = ((val_zero_pos == ::my_zero<decimal_type>()) && (!signbit(val_zero_pos)));

      BOOST_TEST(result_val_zero_pos_is_ok);

      result_is_ok = (result_val_zero_pos_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_zero_neg = sqrt(-::my_zero<decimal_type>());

      const auto result_val_zero_neg_is_ok = ((val_zero_neg == -::my_zero<decimal_type>()) && signbit(val_zero_neg));

      BOOST_TEST(result_val_zero_neg_is_ok);

      result_is_ok = (result_val_zero_neg_is_ok && result_is_ok);
    }

    // Perfect squares: exercises rem==0 branch (no Newton correction needed)
    {
      const auto val_4 = sqrt(static_cast<decimal_type>(4));
      const auto val_9 = sqrt(static_cast<decimal_type>(9));
      const bool result_perfect_sq_is_ok = (val_4 == static_cast<decimal_type>(2)) && (val_9 == static_cast<decimal_type>(3));
      BOOST_TEST(result_perfect_sq_is_ok);
      result_is_ok = (result_perfect_sq_is_ok && result_is_ok);
    }

    // Non-perfect squares: exercises Newton correction block
    {
      const auto val_2 = sqrt(static_cast<decimal_type>(2));
      const auto val_5 = sqrt(static_cast<decimal_type>(5));
      const bool result_non_perfect_is_ok = (val_2 > static_cast<decimal_type>(1)) && (val_2 < static_cast<decimal_type>(2))
                                         && (val_5 > static_cast<decimal_type>(2)) && (val_5 < static_cast<decimal_type>(3));
      BOOST_TEST(result_non_perfect_is_ok);
      result_is_ok = (result_non_perfect_is_ok && result_is_ok);
    }

    // Dense sampling [1.01, 9.99] to potentially hit rem<0 (Newton overshoot) and other edge paths
    #if !defined(BOOST_DECIMAL_REDUCE_TEST_DEPTH)
    {
      bool dense_ok = true;
      // Use relaxed tolerance for decimal128 on platforms where float_type has different
      // precision/rounding (e.g. S390X 128-bit long double) to avoid spurious failures.
      constexpr auto dense_tol_factor = (std::numeric_limits<decimal_type>::digits10 > 16) ? 1024 : 32;
      for (int i = 101; i <= 999; ++i)
      {
        const decimal_type x = static_cast<decimal_type>(i) / static_cast<decimal_type>(100);
        const auto val = sqrt(x);
        using std::sqrt;
        const auto ref = static_cast<float_type>(sqrt(static_cast<float_type>(x)));
        dense_ok = dense_ok && is_close_fraction(static_cast<float_type>(val), ref, std::numeric_limits<float_type>::epsilon() * static_cast<float_type>(dense_tol_factor));
      }
      BOOST_TEST(dense_ok);
      result_is_ok = (dense_ok && result_is_ok);
    }
    #endif

    return result_is_ok;
  }

  auto test_sqrt_128(const int tol_factor) -> bool
  {
    using decimal_type = boost::decimal::decimal128_t;

    using str_ctrl_array_type = std::array<const char*, 41U>;

    const str_ctrl_array_type ctrl_strings =
    {{
       // Table[N[Sqrt[123456 (10^n)], 36], {n, -20, 20, 1}]
       "3.51363060095963986639333846404180558E-8",
       "1.11110755554986664846214940411821923E-7",
       "3.51363060095963986639333846404180558E-7",
       "1.11110755554986664846214940411821923E-6",
       "3.51363060095963986639333846404180558E-6",
       "0.0000111110755554986664846214940411821923",
       "0.0000351363060095963986639333846404180558",
       "0.000111110755554986664846214940411821923",
       "0.000351363060095963986639333846404180558",
       "0.00111110755554986664846214940411821923",
       "0.00351363060095963986639333846404180558",
       "0.0111110755554986664846214940411821923",
       "0.0351363060095963986639333846404180558",
       "0.111110755554986664846214940411821923",
       "0.351363060095963986639333846404180558",
       "1.11110755554986664846214940411821923",
       "3.51363060095963986639333846404180558",
       "11.1110755554986664846214940411821923",
       "35.1363060095963986639333846404180558",
       "111.110755554986664846214940411821923",
       "351.363060095963986639333846404180558",
       "1111.10755554986664846214940411821923",
       "3513.63060095963986639333846404180558",
       "11111.0755554986664846214940411821923",
       "35136.3060095963986639333846404180558",
       "111110.755554986664846214940411821923",
       "351363.060095963986639333846404180558",
       "1.11110755554986664846214940411821923E6",
       "3.51363060095963986639333846404180558E6",
       "1.11110755554986664846214940411821923E7",
       "3.51363060095963986639333846404180558E7",
       "1.11110755554986664846214940411821923E8",
       "3.51363060095963986639333846404180558E8",
       "1.11110755554986664846214940411821923E9",
       "3.51363060095963986639333846404180558E9",
       "1.11110755554986664846214940411821923E10",
       "3.51363060095963986639333846404180558E10",
       "1.11110755554986664846214940411821923E11",
       "3.51363060095963986639333846404180558E11",
       "1.11110755554986664846214940411821923E12",
       "3.51363060095963986639333846404180558E12",
    }};

    std::array<decimal_type, std::tuple_size<str_ctrl_array_type>::value> sqrt_values { };
    std::array<decimal_type, std::tuple_size<str_ctrl_array_type>::value> ctrl_values { };

    int nx { -20 };

    bool result_is_ok { true };

    const decimal_type my_tol { std::numeric_limits<decimal_type>::epsilon() * static_cast<decimal_type>(tol_factor) };

    for(auto i = static_cast<std::size_t>(UINT8_C(0)); i < std::tuple_size<str_ctrl_array_type>::value; ++i)
    {
      const decimal_type x_arg = decimal_type { 123456L, nx };

      ++nx;

      sqrt_values[i] = sqrt(x_arg);

      static_cast<void>
      (
        from_chars(ctrl_strings[i], ctrl_strings[i] + std::strlen(ctrl_strings[i]), ctrl_values[i])
      );

      const auto result_sqrt_is_ok = is_close_fraction(sqrt_values[i], ctrl_values[i], my_tol);

      if (!result_sqrt_is_ok)
      {
        // LCOV_EXCL_START
        constexpr auto prec = std::numeric_limits<decimal_type>::digits10 + 2;
        std::cout << "  [test_sqrt_128 failure] index " << i << ", x_arg = 123456e" << (nx - 1)
                  << "\n  sqrt(x_arg)  : " << std::setprecision(prec) << sqrt_values[i]
                  << "\n  ctrl (expected): " << std::setprecision(prec) << ctrl_values[i]
                  << "\n  tolerance (eps*" << tol_factor << "): " << std::setprecision(prec) << my_tol << std::endl;
        // LCOV_EXCL_STOP
      }

      result_is_ok = (result_sqrt_is_ok && result_is_ok);
    }

    return result_is_ok;
  }

} // namespace local

auto main() -> int
{
  auto result_is_ok = true;

  {
    using decimal_type = boost::decimal::decimal_fast32_t;
    using float_type   = float;

    const auto result_small_is_ok  = local::test_sqrt<decimal_type, float_type>(16, 1.0E-26L, 1.0E-01L);
    const auto result_medium_is_ok = local::test_sqrt<decimal_type, float_type>(16, 0.9E-02L, 1.1E+01L);
    const auto result_large_is_ok  = local::test_sqrt<decimal_type, float_type>(16, 1.0E+01L, 1.0E+26L);

    BOOST_TEST(result_small_is_ok);
    BOOST_TEST(result_medium_is_ok);
    BOOST_TEST(result_large_is_ok);

    const auto result_edge_is_ok = local::test_sqrt_edge<decimal_type, float_type>();

    result_is_ok = (result_small_is_ok && result_medium_is_ok && result_large_is_ok && result_edge_is_ok && result_is_ok);
  }

  {
    using decimal_type = boost::decimal::decimal32_t;
    using float_type   = float;

    const auto result_small_is_ok  = local::test_sqrt<decimal_type, float_type>(16, 1.0E-26L, 1.0E-01L);
    const auto result_medium_is_ok = local::test_sqrt<decimal_type, float_type>(16, 0.9E-02L, 1.1E+01L);
    const auto result_large_is_ok  = local::test_sqrt<decimal_type, float_type>(16, 1.0E+01L, 1.0E+26L);

    BOOST_TEST(result_small_is_ok);
    BOOST_TEST(result_medium_is_ok);
    BOOST_TEST(result_large_is_ok);

    const auto result_edge_is_ok = local::test_sqrt_edge<decimal_type, float_type>();

    const auto result_ranges_is_ok = (result_small_is_ok && result_medium_is_ok && result_large_is_ok);

    result_is_ok = (result_ranges_is_ok && result_is_ok);

    BOOST_TEST(result_edge_is_ok);

    result_is_ok = (result_edge_is_ok && result_is_ok);
  }

  {
    using decimal_type = boost::decimal::decimal64_t;
    using float_type   = double;

    const auto result_small_is_ok  = local::test_sqrt<decimal_type, float_type>(16, 1.0E-76L, 1.0E-01L);
    const auto result_medium_is_ok = local::test_sqrt<decimal_type, float_type>(16, 0.9E-02L, 1.1E+01L);
    const auto result_large_is_ok  = local::test_sqrt<decimal_type, float_type>(16, 1.0E+01L, 1.0E+76L);

    BOOST_TEST(result_small_is_ok);
    BOOST_TEST(result_medium_is_ok);
    BOOST_TEST(result_large_is_ok);

    const auto result_edge_is_ok = local::test_sqrt_edge<decimal_type, float_type>();

    const auto result_ranges_is_ok = (result_small_is_ok && result_medium_is_ok && result_large_is_ok);

    result_is_ok = (result_ranges_is_ok && result_is_ok);

    BOOST_TEST(result_edge_is_ok);

    result_is_ok = (result_edge_is_ok && result_is_ok);
  }

  {
    using decimal_type = boost::decimal::decimal_fast64_t;
    using float_type   = double;

    const auto result_small_is_ok  = local::test_sqrt<decimal_type, float_type>(16, 1.0E-76L, 1.0E-01L);
    const auto result_medium_is_ok = local::test_sqrt<decimal_type, float_type>(16, 0.9E-02L, 1.1E+01L);
    const auto result_large_is_ok  = local::test_sqrt<decimal_type, float_type>(16, 1.0E+01L, 1.0E+76L);

    BOOST_TEST(result_small_is_ok);
    BOOST_TEST(result_medium_is_ok);
    BOOST_TEST(result_large_is_ok);

    const auto result_edge_is_ok = local::test_sqrt_edge<decimal_type, float_type>();

    const auto result_ranges_is_ok = (result_small_is_ok && result_medium_is_ok && result_large_is_ok);

    result_is_ok = (result_ranges_is_ok && result_is_ok);

    BOOST_TEST(result_edge_is_ok);

    result_is_ok = (result_edge_is_ok && result_is_ok);
  }

  {
    const auto result_sqrt128_is_ok = local::test_sqrt_128(16);

    BOOST_TEST(result_sqrt128_is_ok);

    result_is_ok = (result_sqrt128_is_ok && result_is_ok);
  }

  {
    using decimal_type = boost::decimal::decimal128_t;
    // Always use double for decimal128 comparison to avoid decimal128→long double
    // conversion issues on ARM64/S390X (see GitHub issue with 10^15 scale bug).
    using float_type = double;

    // test_sqrt_edge for decimal128_t: covers sqrt128_impl (perfect squares, rem>sig_z, dense sampling)
    const auto result_edge_is_ok = local::test_sqrt_edge<decimal_type, float_type>();

    BOOST_TEST(result_edge_is_ok);

    result_is_ok = (result_edge_is_ok && result_is_ok);
  }

  {
    using decimal_type = boost::decimal::decimal_fast128_t;
    // Always use double for decimal128 comparison to avoid decimal128→long double
    // conversion issues on ARM64/S390X (see GitHub issue with 10^15 scale bug).
    using float_type = double;

    const auto result_small_is_ok  = local::test_sqrt<decimal_type, float_type>(64, 1.0E-76, 1.0E-01);
    const auto result_medium_is_ok = local::test_sqrt<decimal_type, float_type>(64, 0.9E-02, 1.1E+01);
    const auto result_large_is_ok  = local::test_sqrt<decimal_type, float_type>(64, 1.0E+01, 1.0E+76);

    BOOST_TEST(result_small_is_ok);
    BOOST_TEST(result_medium_is_ok);
    BOOST_TEST(result_large_is_ok);

    const auto result_edge_is_ok = local::test_sqrt_edge<decimal_type, float_type>();

    result_is_ok = (result_small_is_ok && result_medium_is_ok && result_large_is_ok && result_edge_is_ok && result_is_ok);
  }

  BOOST_TEST(result_is_ok);

  return boost::report_errors();
}
