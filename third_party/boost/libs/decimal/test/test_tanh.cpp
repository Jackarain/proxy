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

  auto test_tanh(const int tol_factor, const bool negate, const long double range_lo, const long double range_hi) -> bool
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
    constexpr auto count = UINT32_C(0x800);
    #else
    constexpr auto count = UINT32_C(0x80);
    #endif

    for( ; trials < count; ++trials)
    {
      const auto x_flt_begin = dis(gen);

      const auto x_flt = (negate ? -x_flt_begin : x_flt_begin);
      const auto x_dec = static_cast<decimal_type>(x_flt);

      using std::tanh;

      const auto val_flt = tanh(x_flt);
      const auto val_dec = tanh(x_dec);

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

  auto test_tanh_edge() -> bool
  {
    using decimal_type = boost::decimal::decimal32_t;

    std::mt19937_64 gen;

    std::uniform_real_distribution<float> dist(1.01F, 1.04F);

    auto result_is_ok = true;

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_nan = tanh(std::numeric_limits<decimal_type>::quiet_NaN() * static_cast<decimal_type>(dist(gen)));

      const auto result_val_nan_is_ok = isnan(val_nan);

      BOOST_TEST(result_val_nan_is_ok);

      result_is_ok = (result_val_nan_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_inf_pos = tanh(std::numeric_limits<decimal_type>::infinity() * static_cast<decimal_type>(dist(gen)));

      const auto result_val_inf_pos_is_ok = (val_inf_pos == ::my_one());

      BOOST_TEST(result_val_inf_pos_is_ok);

      result_is_ok = (result_val_inf_pos_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_inf_neg = tanh(-std::numeric_limits<decimal_type>::infinity() * static_cast<decimal_type>(dist(gen)));

      const auto result_val_inf_neg_is_ok = (-val_inf_neg == ::my_one());

      BOOST_TEST(result_val_inf_neg_is_ok);

      result_is_ok = (result_val_inf_neg_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_zero_pos = tanh(::my_zero());

      const auto result_val_zero_pos_is_ok = (val_zero_pos == ::my_zero());

      BOOST_TEST(result_val_zero_pos_is_ok);

      result_is_ok = (result_val_zero_pos_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_zero_neg = tanh(-::my_zero());

      const auto result_val_zero_neg_is_ok = (-val_zero_neg == ::my_zero());

      BOOST_TEST(result_val_zero_neg_is_ok);

      result_is_ok = (result_val_zero_neg_is_ok && result_is_ok);
    }

    return result_is_ok;
  }

  auto test_tanh_64(const int tol_factor) -> bool
  {
    using decimal_type = boost::decimal::decimal64_t;

    using val_ctrl_array_type = std::array<double, 19U>;

    const val_ctrl_array_type ctrl_values =
    {{
      // Table[N[Tanh[n/10 + n/100], 17], {n, 1, 19, 1}]
      0.10955847021442953, 0.21651806149302883, 0.31852077690277084,
      0.41364444218713516, 0.50052021119023521, 0.57836341304450574,
      0.64692945044176658, 0.70641932039723524, 0.75736232421652628,
      0.80049902176062971, 0.83667948907681070, 0.86678392884981867,
      0.89166659903752786, 0.91212036920771735, 0.92885762145472765,
      0.94250300814692005, 0.95359412370871184, 0.96258698009129079,
      0.96986402037881437
    }};

    std::array<decimal_type, std::tuple_size<val_ctrl_array_type>::value> tanh_values { };

    int nx { 1 };

    bool result_is_ok { true };

    const decimal_type my_tol { std::numeric_limits<decimal_type>::epsilon() * static_cast<decimal_type>(tol_factor) };

    for(auto i = static_cast<std::size_t>(UINT8_C(0)); i < std::tuple_size<val_ctrl_array_type>::value; ++i)
    {
      // Table[N[Tanh[n/10 + n/100], 17], {n, 1, 19, 1}]

      const decimal_type
        x_arg
        {
            decimal_type { nx, -1 }
          + decimal_type { nx, -2 }
        };

      tanh_values[i] = tanh(x_arg);

      ++nx;

      const auto result_tanh_is_ok = is_close_fraction(tanh_values[i], decimal_type(ctrl_values[i]), my_tol);

      result_is_ok = (result_tanh_is_ok && result_is_ok);
    }

    return result_is_ok;
  }

  auto test_tanh_128(const int tol_factor) -> bool
  {
    using decimal_type = boost::decimal::decimal128_t;

    using str_ctrl_array_type = std::array<const char*, 19U>;

    const str_ctrl_array_type ctrl_strings =
    {{
       // Table[N[Tanh[n/10 + n/100], 36], {n, 1, 19, 1}]
       "0.109558470214429529083953711078453335",
       "0.216518061493028830952237517928704430",
       "0.318520776902770841524226142705201206",
       "0.413644442187135160181149867511604876",
       "0.500520211190235208419953684824125801",
       "0.578363413044505744966048013932497177",
       "0.646929450441766577325986651436349519",
       "0.706419320397235235044602587095137949",
       "0.757362324216526281517465173132116408",
       "0.800499021760629706011461330600696458",
       "0.836679489076810698393340891858465536",
       "0.866783928849818673011673209608596967",
       "0.891666599037527863908869407309843691",
       "0.912120369207717348605844324504928728",
       "0.928857621454727654445287051918743728",
       "0.942503008146920053439238800146334840",
       "0.953594123708711839166836669769523780",
       "0.962586980091290794379928292346757460",
       "0.969864020378814366065021209657656364",
    }};

    std::array<decimal_type, std::tuple_size<str_ctrl_array_type>::value> tanh_values { };
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

      tanh_values[i] = tanh(x_arg);

      static_cast<void>
      (
        from_chars(ctrl_strings[i], ctrl_strings[i] + std::strlen(ctrl_strings[i]), ctrl_values[i])
      );

      const auto result_tanh_is_ok = is_close_fraction(tanh_values[i], ctrl_values[i], my_tol);

      result_is_ok = (result_tanh_is_ok && result_is_ok);
    }

    return result_is_ok;
  }

} // namespace local

auto main() -> int
{
  auto result_is_ok = true;

  const auto result_pos_is_ok = local::test_tanh(96, false, 0.03125L, 32.0L);
  const auto result_neg_is_ok = local::test_tanh(96, true,  0.03125L, 32.0L);

  const auto result_pos_narrow_is_ok = local::test_tanh(24, false, 0.125L, 8.0L);
  const auto result_neg_narrow_is_ok = local::test_tanh(24, true,  0.125L, 8.0L);

  const auto result_pos_wide_is_ok = local::test_tanh(128, false, 0.015625L, 64.0L);
  const auto result_neg_wide_is_ok = local::test_tanh(128, true,  0.015625L, 64.0L);

  const auto result_edge_is_ok = local::test_tanh_edge();

  const auto result_pos64_is_ok = local::test_tanh_64(64);

  const auto result_pos128_is_ok = local::test_tanh_128(8192);

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
