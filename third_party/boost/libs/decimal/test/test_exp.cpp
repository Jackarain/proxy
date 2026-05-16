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

  template<typename DecimalType, typename FloatType>
  auto test_exp(const int tol_factor, const bool negate, const long double range_lo, const long double range_hi) -> bool
  {
    using decimal_type = DecimalType;
    using float_type   = FloatType;

    std::random_device rd;
    std::mt19937_64 gen(rd());

    gen.seed(time_point<typename std::mt19937_64::result_type>());

    auto dis =
      std::uniform_real_distribution<float_type>
      {
        static_cast<float_type>(range_lo),
        static_cast<float_type>(range_hi)
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
      const auto x_flt_begin = dis(gen);

      const auto x_flt = (negate ? -x_flt_begin : x_flt_begin);
      const auto x_dec = static_cast<decimal_type>(x_flt);

      using std::exp;

      const auto val_flt = exp(x_flt);
      const auto val_dec = exp(x_dec);

      const auto result_val_is_ok = is_close_fraction(val_flt, static_cast<float_type>(val_dec), static_cast<float_type>(std::numeric_limits<decimal_type>::epsilon()) * static_cast<float_type>(tol_factor));

      result_is_ok = (result_val_is_ok && result_is_ok);

      if(!result_val_is_ok)
      {
        // LCOV_EXCL_START
        std::cerr << "x_flt  : " << std::scientific << std::setprecision(std::numeric_limits<float_type>::digits10) << x_flt   << std::endl;
        std::cerr << "val_flt: " << std::scientific << std::setprecision(std::numeric_limits<float_type>::digits10) << val_flt << std::endl;
        std::cerr << "val_dec: " << std::scientific << std::setprecision(std::numeric_limits<float_type>::digits10) << val_dec << std::endl;

        break;
        // LCOV_EXCL_STOP
      }
    }

    BOOST_TEST(result_is_ok);

    return result_is_ok;
  }

  template<typename DecimalType, typename FloatType>
  auto test_exp_edge() -> bool
  {
    using decimal_type = DecimalType;
    using float_type   = FloatType;

    std::mt19937_64 gen;

    std::uniform_real_distribution<float_type>
      dist
      (
        static_cast<float_type>(1.01L),
        static_cast<float_type>(1.04L)
      );

    auto result_is_ok = true;

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_nan = exp(std::numeric_limits<decimal_type>::quiet_NaN() * static_cast<decimal_type>(dist(gen)));

      const auto result_val_nan_is_ok = isnan(val_nan);

      BOOST_TEST(result_val_nan_is_ok);

      result_is_ok = (result_val_nan_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const decimal_type arg_inf { std::numeric_limits<decimal_type>::infinity() * static_cast<decimal_type>(dist(gen)) };

      const auto val_inf_pos = exp(arg_inf);

      const auto result_val_inf_pos_is_ok = (fpclassify(val_inf_pos) == FP_INFINITE);

      BOOST_TEST(result_val_inf_pos_is_ok);

      result_is_ok = (result_val_inf_pos_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_inf_neg = exp(-std::numeric_limits<decimal_type>::infinity() * static_cast<decimal_type>(dist(gen)));

      const auto result_val_inf_neg_is_ok = (val_inf_neg == ::my_zero<decimal_type>());

      BOOST_TEST(result_val_inf_neg_is_ok);

      result_is_ok = (result_val_inf_neg_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_zero_pos = exp(::my_zero<decimal_type>());

      const auto result_val_zero_pos_is_ok = (val_zero_pos == ::my_one<decimal_type>());

      BOOST_TEST(result_val_zero_pos_is_ok);

      result_is_ok = (result_val_zero_pos_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_zero_neg = exp(-::my_zero<decimal_type>());

      const auto result_val_zero_neg_is_ok = (val_zero_neg == ::my_one<decimal_type>());

      BOOST_TEST(result_val_zero_neg_is_ok);

      result_is_ok = (result_val_zero_neg_is_ok && result_is_ok);
    }

    return result_is_ok;
  }

  auto test_exp_128(const int tol_factor) -> bool
  {
    using decimal_type = boost::decimal::decimal128_t;

    using str_ctrl_array_type = std::array<const char*, 39U>;

    const str_ctrl_array_type ctrl_strings =
    {{
       // Table[N[Exp[n/10 + n/100], 36], {n, 1, 39, 1}]
       "1.11627807045887129150073776905298390",
       "1.24607673058738081952026478299269624",
       "1.39096812846378026624274780495311882",
       "1.55270721851133604205007964619169497",
       "1.73325301786739523682191676713732884",
       "1.93479233440203152169312515101969168",
       "2.15976625378491500838755239034002685",
       "2.41089970641720985089088491613290280",
       "2.69123447234926228909987940407101397",
       "3.00416602394643311205840795358867239",
       "3.35348465254902368100358942737571204",
       "3.74342137726086256855805582982587323",
       "4.17869919192324615658039176435293801",
       "4.66459027098812590279338676624377783",
       "5.20697982717984873765730709271233513",
       "5.81243739440258864988034062444969445",
       "6.48829639928671111502903132434912956",
       "7.24274298516101220851243475314474762",
       "8.08491516430506017497344071644188155",
       "9.02501349943412092647177716688866403",
       "10.0744246550135862002454552896844711",
       "11.2458593148818460799615892055305690",
       "12.5535061366682314080320232000754142",
       "14.0132036077336131602667577975340025",
       "15.6426318841881716102126980461566588",
       "17.4615269365799904170450682499698346",
       "19.4919195960311175203209452590133521",
       "21.7584023961970778443863882601062266",
       "24.2884274430945556043070982961719396",
       "27.1126389206578874268183721102312223",
       "30.2652442594000813446015323588968824",
       "33.7844284638495538820910085630299049",
       "37.7128166171817490996824895604598120",
       "42.0979901649969005914744807079465071",
       "46.9930632315792808648304762411623248",
       "52.4573259490990503124315131185087067",
       "58.5569625918923670285321923410850419",
       "65.3658532140099181652435900015868107",
       "72.9664684996328018947164376727604433",
    }};

    std::array<decimal_type, std::tuple_size<str_ctrl_array_type>::value> exp_values { };
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

      exp_values[i] = exp(x_arg);

      static_cast<void>
      (
        from_chars(ctrl_strings[i], ctrl_strings[i] + std::strlen(ctrl_strings[i]), ctrl_values[i])
      );

      const auto result_exp_is_ok = is_close_fraction(exp_values[i], ctrl_values[i], my_tol);

      result_is_ok = (result_exp_is_ok && result_is_ok);
    }

    return result_is_ok;
  }

} // namespace local

auto main() -> int
{
  auto result_is_ok = true;

  {
    using decimal_type = boost::decimal::decimal32_t;
    using float_type   = float;

    const auto result_pos_is_ok = local::test_exp<decimal_type, float_type>(128, false, 0.03125L, 80.0L);
    const auto result_neg_is_ok = local::test_exp<decimal_type, float_type>(128, true,  0.03125L, 80.0L);

    const auto result_pos_narrow_is_ok = local::test_exp<decimal_type, float_type>(64, false, 0.25L, 4.0L);
    const auto result_neg_narrow_is_ok = local::test_exp<decimal_type, float_type>(64, true,  0.25L, 4.0L);

    const auto result_edge_is_ok = local::test_exp_edge<decimal_type, float_type>();

    BOOST_TEST(result_pos_is_ok);
    BOOST_TEST(result_neg_is_ok);

    BOOST_TEST(result_pos_narrow_is_ok);
    BOOST_TEST(result_neg_narrow_is_ok);

    BOOST_TEST(result_edge_is_ok);

    result_is_ok = (result_pos_is_ok && result_is_ok);
    result_is_ok = (result_neg_is_ok && result_is_ok);

    result_is_ok = (result_pos_narrow_is_ok && result_is_ok);
    result_is_ok = (result_neg_narrow_is_ok && result_is_ok);

    result_is_ok = (result_edge_is_ok && result_is_ok);
  }

  {
    using decimal_type = boost::decimal::decimal64_t;
    using float_type   = double;

    const auto result_pos_lo_is_ok = local::test_exp<decimal_type, float_type>(512, false, 0.03125L, 80.0L);
    const auto result_neg_lo_is_ok = local::test_exp<decimal_type, float_type>(512, true,  0.03125L, 80.0L);

    const auto result_pos_hi_is_ok = local::test_exp<decimal_type, float_type>(2048, false, 8.0L, 512.0L);
    const auto result_neg_hi_is_ok = local::test_exp<decimal_type, float_type>(2048, true,  8.0L, 512.0L);

    const auto result_edge_is_ok = local::test_exp_edge<decimal_type, float_type>();

    BOOST_TEST(result_pos_lo_is_ok);
    BOOST_TEST(result_neg_lo_is_ok);

    BOOST_TEST(result_pos_hi_is_ok);
    BOOST_TEST(result_neg_hi_is_ok);

    BOOST_TEST(result_edge_is_ok);

    result_is_ok = (result_pos_lo_is_ok && result_is_ok);
    result_is_ok = (result_neg_lo_is_ok && result_is_ok);

    result_is_ok = (result_pos_hi_is_ok && result_is_ok);
    result_is_ok = (result_neg_hi_is_ok && result_is_ok);

    result_is_ok = (result_edge_is_ok && result_is_ok);
  }

  {
    const auto result_pos128_is_ok = local::test_exp_128(8192);

    BOOST_TEST(result_pos128_is_ok);

    result_is_ok = (result_pos128_is_ok && result_is_ok);
  }

  result_is_ok = ((boost::report_errors() == 0) && result_is_ok);

  return (result_is_ok ? 0 : -1);
}
