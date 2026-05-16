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

auto my_make_nan(boost::decimal::decimal32_t factor) -> boost::decimal::decimal32_t;

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

  auto test_log1p(const int tol_factor, const bool negate, const long double range_lo, const long double range_hi) -> bool
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

      using std::log1p;

      const auto val_flt = log1p(x_flt);
      const auto val_dec = log1p(x_dec);

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

  auto test_log1p_edge() -> bool
  {
    using decimal_type = boost::decimal::decimal32_t;

    std::mt19937_64 gen;

    std::uniform_real_distribution<float> dist(1.01F, 1.04F);

    auto result_is_ok = true;

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(64)); ++i)
    {
      static_cast<void>(i);

      const auto arg_nan = my_make_nan(static_cast<decimal_type>(dist(gen)));

      const auto val_nan = log1p(arg_nan);

      const auto result_val_nan_is_ok = (isnan(arg_nan) && isnan(val_nan));

      BOOST_TEST(result_val_nan_is_ok);

      result_is_ok = (result_val_nan_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_inf_pos = log1p(std::numeric_limits<decimal_type>::infinity() * static_cast<decimal_type>(dist(gen)));

      const auto result_val_inf_pos_is_ok = isinf(val_inf_pos);

      BOOST_TEST(result_val_inf_pos_is_ok);

      result_is_ok = (result_val_inf_pos_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_inf_neg = log1p(-std::numeric_limits<decimal_type>::infinity() * static_cast<decimal_type>(dist(gen)));

      const auto result_val_inf_neg_is_ok = isnan(val_inf_neg);

      BOOST_TEST(result_val_inf_neg_is_ok);

      result_is_ok = (result_val_inf_neg_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_zero_pos = log1p(::my_zero());

      const auto result_val_zero_pos_is_ok = (val_zero_pos == ::my_zero());

      BOOST_TEST(result_val_zero_pos_is_ok);

      result_is_ok = (result_val_zero_pos_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_zero_neg = log1p(-::my_zero());

      const auto result_val_zero_neg_is_ok = (-val_zero_neg == ::my_zero());

      BOOST_TEST(result_val_zero_neg_is_ok);

      result_is_ok = (result_val_zero_neg_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_one_minus = log1p(-::my_one());

      const auto result_val_one_minus_is_ok = (isinf(val_one_minus) && signbit(val_one_minus));

      BOOST_TEST(result_val_one_minus_is_ok);

      result_is_ok = (result_val_one_minus_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_something_minus = log1p(-(::my_one() + ::my_one()) * static_cast<decimal_type>(dist(gen)));

      const auto result_val_something_minus_is_ok = isnan(val_something_minus);

      BOOST_TEST(result_val_something_minus_is_ok);

      result_is_ok = (result_val_something_minus_is_ok && result_is_ok);
    }

    return result_is_ok;
  }

  auto test_log1p_64(const int tol_factor) -> bool
  {
    using decimal_type = boost::decimal::decimal64_t;

    using val_ctrl_array_type = std::array<double, 21U>;

    const val_ctrl_array_type ctrl_values =
    {{
      // Table[N[Log[1 + n/100], 17], {n, -10, 10, 1}]
      -0.10536051565782630,  -0.094310679471241327, -0.083381608939051058,
      -0.072570692834835431, -0.061875403718087472, -0.051293294387550533,
      -0.040821994520255130, -0.030459207484708546, -0.020202707317519448,
      -0.010050335853501441,  0,                     0.0099503308531680828,
       0.019802627296179713,  0.029558802241544403,  0.039220713153281296,
       0.048790164169432003,  0.058268908123975776,  0.067658648473814805,
       0.076961041136128325,  0.086177696241052332,  0.095310179804324860
    }};

    std::array<decimal_type, std::tuple_size<val_ctrl_array_type>::value> log1p_values { };

    int nx { -10 };

    bool result_is_ok { true };

    const decimal_type my_tol { std::numeric_limits<decimal_type>::epsilon() * static_cast<decimal_type>(tol_factor) };

    for(auto i = static_cast<std::size_t>(UINT8_C(0)); i < std::tuple_size<val_ctrl_array_type>::value; ++i)
    {
      // Table[N[Log[1 + n/100], 17], {n, -1, 10, 1}]

      const decimal_type x_arg { nx, -2 };

      log1p_values[i] = log1p(x_arg);

      ++nx;

      const auto result_log1p_is_ok = is_close_fraction(log1p_values[i], decimal_type(ctrl_values[i]), my_tol);

      result_is_ok = (result_log1p_is_ok && result_is_ok);
    }

    return result_is_ok;
  }

  auto test_log1p_128(const int tol_factor) -> bool
  {
    using decimal_type = boost::decimal::decimal128_t;

    using str_ctrl_array_type = std::array<const char*, 21U>;

    const str_ctrl_array_type ctrl_strings =
    {{
        // Table[N[Log[1 + n/100], 36], {n, -1, 10, 1}]
       "-0.105360515657826301227500980839312798",
       "-0.0943106794712413268771427243602300808",
       "-0.0833816089390510583947658346421791606",
       "-0.0725706928348354307115733479038455001",
       "-0.0618754037180874717978001181383781382",
       "-0.0512932943875505334261961442546872384",
       "-0.0408219945202551295545770651553198702",
       "-0.0304592074847085459192612876647667014",
       "-0.0202027073175194484080453010241923879",
       "-0.0100503358535014411835488575585477061",
       "0",
       "0.00995033085316808284821535754426074169",
       "0.0198026272961797130260290668851003931",
       "0.0295588022415444027326194056847124054",
       "0.0392207131532812962692008965711198938",
       "0.0487901641694320030653744042231646586",
       "0.0582689081239757755257183511185059232",
       "0.0676586484738148052684159076545485864",
       "0.0769610411361283249842170443152018349",
       "0.0861776962410523323413335428404732359",
       "0.0953101798043248600439521232807650922",
    }};

    std::array<decimal_type, std::tuple_size<str_ctrl_array_type>::value> log1p_values { };
    std::array<decimal_type, std::tuple_size<str_ctrl_array_type>::value> ctrl_values { };

    int nx { -10 };

    bool result_is_ok { true };

    const decimal_type my_tol { std::numeric_limits<decimal_type>::epsilon() * static_cast<decimal_type>(tol_factor) };

    for(auto i = static_cast<std::size_t>(UINT8_C(0)); i < std::tuple_size<str_ctrl_array_type>::value; ++i)
    {
      const decimal_type x_arg { nx, -2 };

      ++nx;

      log1p_values[i] = log1p(x_arg);

      static_cast<void>
      (
        from_chars(ctrl_strings[i], ctrl_strings[i] + std::strlen(ctrl_strings[i]), ctrl_values[i])
      );

      const auto result_log1p_is_ok = is_close_fraction(log1p_values[i], ctrl_values[i], my_tol);

      result_is_ok = (result_log1p_is_ok && result_is_ok);
    }

    return result_is_ok;
  }

} // namespace local

auto main() -> int
{
  auto result_is_ok = true;

  const auto result_pos_is_ok = local::test_log1p(96, false, 0.0L, 2.0L);

  const auto result_narrow_is_ok = local::test_log1p(16, false, -0.375L, 0.375L);

  const auto result_pos_wide_is_ok = local::test_log1p(96, false, 1.0L, 1.0E6L);

  const auto result_edge_is_ok = local::test_log1p_edge();

  BOOST_TEST(result_pos_is_ok);
  BOOST_TEST(result_narrow_is_ok);
  BOOST_TEST(result_pos_wide_is_ok);
  BOOST_TEST(result_edge_is_ok);

  result_is_ok = (result_pos_is_ok      && result_is_ok);
  result_is_ok = (result_narrow_is_ok   && result_is_ok);
  result_is_ok = (result_pos_wide_is_ok && result_is_ok);
  result_is_ok = (result_edge_is_ok     && result_is_ok);

  {
    const auto result_pos64_is_ok = local::test_log1p_64(64);

    BOOST_TEST(result_pos64_is_ok);

    result_is_ok = (result_pos64_is_ok && result_is_ok);
  }

  {
    const auto result_pos128_is_ok = local::test_log1p_128(8192);

    BOOST_TEST(result_pos128_is_ok);

    result_is_ok = (result_pos128_is_ok && result_is_ok);
  }

  result_is_ok = ((boost::report_errors() == 0) && result_is_ok);

  return (result_is_ok ? 0 : -1);
}

auto my_zero() -> boost::decimal::decimal32_t& { static boost::decimal::decimal32_t val_zero { 0, 0 }; return val_zero; }
auto my_one () -> boost::decimal::decimal32_t& { static boost::decimal::decimal32_t val_one  { 1, 0 }; return val_one; }

auto my_make_nan(boost::decimal::decimal32_t factor) -> boost::decimal::decimal32_t
{
  boost::decimal::decimal32_t val_nan = std::numeric_limits<boost::decimal::decimal32_t>::quiet_NaN();

  return val_nan * factor;
}
