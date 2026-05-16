// Copyright 2023 - 2024 Matt Borland
// Copyright 2023 - 2024 Christopher Kormanyos
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

    NumericType delta { };

    if(b == static_cast<NumericType>(0))
    {
      delta = fabs(a - b); // LCOV_EXCL_LINE

      result_is_ok = (delta < tol); // LCOV_EXCL_LINE
    }
    else
    {
      delta = fabs(1 - (a / b));

      result_is_ok = (delta < tol);
    }

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

  auto test_expm1(const int tol_factor, const bool negate, const long double range_lo, const long double range_hi) -> bool
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

      using std::expm1;

      const auto val_flt = expm1(x_flt);
      const auto val_dec = expm1(x_dec);

      const auto result_val_is_ok = is_close_fraction(val_flt, static_cast<float>(val_dec), std::numeric_limits<float>::epsilon() * static_cast<float>(tol_factor));

      result_is_ok = (result_val_is_ok && result_is_ok);

      if(!result_val_is_ok)
      {
          // LCOV_EXCL_START
        std::cerr << "x_flt  : " <<                    x_flt   << std::endl;
        std::cerr << "val_flt: " << std::scientific << val_flt << std::endl;
        std::cerr << "val_dec: " << std::scientific << val_dec << std::endl;

        break;
          // LCOV_EXCL_STOP
      }
    }

    BOOST_TEST(result_is_ok);

    return result_is_ok;
  }

  auto test_expm1_edge() -> bool
  {
    using decimal_type = boost::decimal::decimal32_t;

    std::mt19937_64 gen;

    std::uniform_real_distribution<float> dist(1.01F, 1.04F);

    auto result_is_ok = true;

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_nan = expm1(std::numeric_limits<decimal_type>::quiet_NaN() * static_cast<decimal_type>(dist(gen)));

      const auto result_val_nan_is_ok = isnan(val_nan);

      BOOST_TEST(result_val_nan_is_ok);

      result_is_ok = (result_val_nan_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_inf_pos = expm1(std::numeric_limits<decimal_type>::infinity() * static_cast<decimal_type>(dist(gen)));

      const auto result_val_inf_pos_is_ok = isinf(val_inf_pos);

      BOOST_TEST(result_val_inf_pos_is_ok);

      result_is_ok = (result_val_inf_pos_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_inf_neg = expm1(-std::numeric_limits<decimal_type>::infinity() * static_cast<decimal_type>(dist(gen)));

      const auto result_val_inf_neg_is_ok = (-val_inf_neg == ::my_one());

      BOOST_TEST(result_val_inf_neg_is_ok);

      result_is_ok = (result_val_inf_neg_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_zero_pos = expm1(::my_zero());

      const auto result_val_zero_pos_is_ok = (val_zero_pos == ::my_zero());

      BOOST_TEST(result_val_zero_pos_is_ok);

      result_is_ok = (result_val_zero_pos_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_zero_neg = expm1(-::my_zero());

      const auto result_val_zero_neg_is_ok = (-val_zero_neg == ::my_zero());

      BOOST_TEST(result_val_zero_neg_is_ok);

      result_is_ok = (result_val_zero_neg_is_ok && result_is_ok);
    }

    return result_is_ok;
  }

  auto test_expm1_64(const int tol_factor) -> bool
  {
    using decimal_type = boost::decimal::decimal64_t;

    using val_ctrl_array_type = std::array<double, 10U>;

    const val_ctrl_array_type ctrl_values =
    {{
      // Table[N[Exp[n/10 + n/100] - 1, 17], {n, 1, 10, 1}]
      0.11627807045887129, 0.24607673058738082, 0.39096812846378027,
      0.55270721851133604, 0.73325301786739524, 0.93479233440203152,
      1.1597662537849150,  1.4108997064172099,  1.6912344723492623,
      2.0041660239464331
    }};

    std::array<decimal_type, std::tuple_size<val_ctrl_array_type>::value> expm1_values { };

    int nx { 1 };

    bool result_is_ok { true };

    const decimal_type my_tol { std::numeric_limits<decimal_type>::epsilon() * static_cast<decimal_type>(tol_factor) };

    for(auto i = static_cast<std::size_t>(UINT8_C(0)); i < std::tuple_size<val_ctrl_array_type>::value; ++i)
    {
      // Table[N[Exp[n/10 + n/100] - 1, 17], {n, 1, 10, 1}]

      const decimal_type
        x_arg
        {
            decimal_type { nx, -1 }
          + decimal_type { nx, -2 }
        };

      expm1_values[i] = expm1(x_arg);

      ++nx;

      const auto result_expm1_is_ok = is_close_fraction(expm1_values[i], decimal_type(ctrl_values[i]), my_tol);

      result_is_ok = (result_expm1_is_ok && result_is_ok);
    }

    return result_is_ok;
  }

  auto test_expm1_128(const int tol_factor) -> bool
  {
    using decimal_type = boost::decimal::decimal128_t;

    using str_ctrl_array_type = std::array<const char*, 10U>;

    const str_ctrl_array_type ctrl_strings =
    {{
       // Table[N[Exp[n/10 + n/100] - 1, 36], {n, 1, 10, 1}]
       "0.116278070458871291500737769052983899",
       "0.246076730587380819520264782992696244",
       "0.390968128463780266242747804953118824",
       "0.552707218511336042050079646191694969",
       "0.733253017867395236821916767137328837",
       "0.934792334402031521693125151019691675",
       "1.15976625378491500838755239034002685",
       "1.41089970641720985089088491613290280",
       "1.69123447234926228909987940407101397",
       "2.00416602394643311205840795358867239",
    }};

    std::array<decimal_type, std::tuple_size<str_ctrl_array_type>::value> expm1_values { };
    std::array<decimal_type, std::tuple_size<str_ctrl_array_type>::value> ctrl_values { };

    int nx { 1 };

    bool result_is_ok { true };

    const decimal_type my_tol { std::numeric_limits<decimal_type>::epsilon() * static_cast<decimal_type>(tol_factor) };

    for(auto i = static_cast<std::size_t>(UINT8_C(0)); i < std::tuple_size<str_ctrl_array_type>::value; ++i)
    {
      const decimal_type
        x_arg
        {
            decimal_type { nx, -1 }
          + decimal_type { nx, -2 }
        };

      ++nx;

      expm1_values[i] = expm1(x_arg);

      static_cast<void>
      (
        from_chars(ctrl_strings[i], ctrl_strings[i] + std::strlen(ctrl_strings[i]), ctrl_values[i])
      );

      const auto result_expm1_is_ok = is_close_fraction(expm1_values[i], ctrl_values[i], my_tol);

      result_is_ok = (result_expm1_is_ok && result_is_ok);
    }

    return result_is_ok;
  }

} // namespace local

auto main() -> int
{
  auto result_is_ok = true;

  const auto result_pos_is_ok = local::test_expm1(128, false, 0.03125L, 32.0L);
  const auto result_neg_is_ok = local::test_expm1(128, true,  0.03125L, 32.0L);

  const auto result_pos_narrow_is_ok = local::test_expm1(24, false, 0.125L, 8.0L);
  const auto result_neg_narrow_is_ok = local::test_expm1(24, true,  0.125L, 8.0L);

  const auto result_pos_wide_is_ok = local::test_expm1(128, false, 0.015625L, 64.0L);
  const auto result_neg_wide_is_ok = local::test_expm1(128, true,  0.015625L, 64.0L);

  const auto result_edge_is_ok = local::test_expm1_edge();

  const auto result_pos64_is_ok = local::test_expm1_64(64);

  const auto result_pos128_is_ok = local::test_expm1_128(8192);

  BOOST_TEST(result_pos_is_ok);
  BOOST_TEST(result_neg_is_ok);

  BOOST_TEST(result_pos_narrow_is_ok);
  BOOST_TEST(result_neg_narrow_is_ok);

  BOOST_TEST(result_pos_wide_is_ok);
  BOOST_TEST(result_neg_wide_is_ok);

  BOOST_TEST(result_edge_is_ok);

  BOOST_TEST(result_pos64_is_ok);

  BOOST_TEST(result_pos128_is_ok);

  result_is_ok = (result_pos_is_ok  && result_is_ok);
  result_is_ok = (result_neg_is_ok  && result_is_ok);

  result_is_ok = (result_pos_narrow_is_ok  && result_is_ok);
  result_is_ok = (result_neg_narrow_is_ok  && result_is_ok);

  result_is_ok = (result_pos_wide_is_ok  && result_is_ok);
  result_is_ok = (result_neg_wide_is_ok  && result_is_ok);

  result_is_ok = (result_edge_is_ok && result_is_ok);

  result_is_ok = (result_pos64_is_ok && result_is_ok);

  result_is_ok = (result_pos128_is_ok && result_is_ok);

  result_is_ok = ((boost::report_errors() == 0) && result_is_ok);

  return (result_is_ok ? 0 : -1);
}

auto my_zero() -> boost::decimal::decimal32_t& { static boost::decimal::decimal32_t val_zero { 0, 0 }; return val_zero; }
auto my_one () -> boost::decimal::decimal32_t& { static boost::decimal::decimal32_t val_one  { 1, 0 }; return val_one; }
