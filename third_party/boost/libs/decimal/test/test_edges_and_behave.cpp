// Copyright 2023 Matt Borland
// Copyright 2023 Christopher Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include "testing_config.hpp"
#include <chrono>
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
auto my_inf () -> boost::decimal::decimal32_t&;
auto my_nan () -> boost::decimal::decimal32_t&;
auto my_pi  () -> boost::decimal::decimal32_t&;
auto my_a   () -> boost::decimal::decimal32_t&;
auto my_b   () -> boost::decimal::decimal32_t&;

namespace local
{
  std::mt19937_64 gen;

  std::uniform_real_distribution<float> dist(1.01F, 1.04F);

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
  auto is_close_fraction(const NumericType a,
                         const NumericType b,
                         const NumericType tol) noexcept -> bool
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

  auto test_behave_over_under() -> bool
  {
    using decimal_type = boost::decimal::decimal32_t;

    auto result_is_ok = true;

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto local_nan_to_construct_f  = decimal_type { std::numeric_limits<float>::quiet_NaN() * dist(gen) };
      const auto local_nan_to_construct_d  = decimal_type { std::numeric_limits<double>::quiet_NaN() * static_cast<double>(dist(gen)) };

      #ifndef BOOST_DECIMAL_UNSUPPORTED_LONG_DOUBLE
      const auto local_nan_to_construct_ld = decimal_type { std::numeric_limits<long double>::quiet_NaN() * static_cast<long double>(dist(gen)) };
      #endif

      const auto result_nan_construct_is_ok =
        (
             isnan(local_nan_to_construct_f)
          && isnan(local_nan_to_construct_d)
          #ifndef BOOST_DECIMAL_UNSUPPORTED_LONG_DOUBLE
          && isnan(local_nan_to_construct_ld)
          #endif
        );

      BOOST_TEST(result_nan_construct_is_ok);

      result_is_ok = (result_nan_construct_is_ok && result_is_ok);

      {
        const auto sum_nan_01 = local_nan_to_construct_f + 1;
        const auto sum_nan_02 = local_nan_to_construct_f + decimal_type { 2, 0 };
        const auto sum_nan_03 = local_nan_to_construct_f + decimal_type { 3.0 };

        const auto result_sum_nan_is_ok = (isnan(sum_nan_01) && isnan(sum_nan_02) && isnan(sum_nan_03));

        BOOST_TEST(result_sum_nan_is_ok);

        result_is_ok = (result_sum_nan_is_ok && result_is_ok);
      }

      {
        const auto dif_nan_01 = local_nan_to_construct_f - 1;
        const auto dif_nan_02 = local_nan_to_construct_f - decimal_type { 2, 0 };
        const auto dif_nan_03 = local_nan_to_construct_f - decimal_type { 3.0 };

        const auto result_dif_nan_is_ok = (isnan(dif_nan_01) && isnan(dif_nan_02) && isnan(dif_nan_03));

        BOOST_TEST(result_dif_nan_is_ok);

        result_is_ok = (result_dif_nan_is_ok && result_is_ok);
      }
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto local_inf_lhs  = ::my_inf() * static_cast<decimal_type>(dist(gen));
      const auto local_inf_rhs  = ::my_inf() * static_cast<decimal_type>(dist(gen));

      const auto local_one = ::my_one();

      {
        const auto sum_inf_dec_inf_dec = local_inf_lhs + local_inf_rhs;

        const auto result_sum_inf_dec_inf_dec_is_ok = isinf(sum_inf_dec_inf_dec);

        BOOST_TEST(result_sum_inf_dec_inf_dec_is_ok);

        result_is_ok = (result_sum_inf_dec_inf_dec_is_ok && result_is_ok);
      }

      {
        const auto sum_one_dec_inf_dec = local_one + local_inf_rhs;

        const auto result_sum_one_dec_inf_dec_is_ok = isinf(sum_one_dec_inf_dec);

        BOOST_TEST(result_sum_one_dec_inf_dec_is_ok);

        result_is_ok = (result_sum_one_dec_inf_dec_is_ok && result_is_ok);
      }

      {
        const auto sum_inf_dec_one_dec = local_inf_lhs + local_one;

        const auto result_sum_inf_dec_one_dec_is_ok = isinf(sum_inf_dec_one_dec);

        BOOST_TEST(result_sum_inf_dec_one_dec_is_ok);

        result_is_ok = (result_sum_inf_dec_one_dec_is_ok && result_is_ok);
      }

      {
        const auto result_cmp_01_is_ok = (local_inf_lhs > local_one);
        const auto result_cmp_02_is_ok = (local_one < local_inf_rhs);

        const auto result_cmp_is_ok = (result_cmp_01_is_ok && result_cmp_02_is_ok);

        BOOST_TEST(result_cmp_is_ok);

        result_is_ok = (result_cmp_is_ok && result_is_ok);
      }

      {
        const auto result_cmp_01_is_ok = (-local_inf_lhs < local_one);
        const auto result_cmp_02_is_ok = (local_one > -local_inf_rhs);

        const auto result_cmp_is_ok = (result_cmp_01_is_ok && result_cmp_02_is_ok);

        BOOST_TEST(result_cmp_is_ok);

        result_is_ok = (result_cmp_is_ok && result_is_ok);
      }

      {
        const auto result_cmp_01_is_ok = (local_inf_lhs > 1);
        const auto result_cmp_02_is_ok = (1 < local_inf_rhs);

        const auto result_cmp_is_ok = (result_cmp_01_is_ok && result_cmp_02_is_ok);

        BOOST_TEST(result_cmp_is_ok);

        result_is_ok = (result_cmp_is_ok && result_is_ok);
      }

      {
        const auto result_cmp_01_is_ok = (-local_inf_lhs < 1);
        const auto result_cmp_02_is_ok = (1 > -local_inf_rhs);

        const auto result_cmp_is_ok = (result_cmp_01_is_ok && result_cmp_02_is_ok);

        BOOST_TEST(result_cmp_is_ok);

        result_is_ok = (result_cmp_is_ok && result_is_ok);
      }

      {
        const auto result_div_01 = (local_inf_lhs /  local_one);
        const auto result_div_02 = (local_inf_lhs / -local_one);

        const auto result_div_01_is_ok = isinf(result_div_01) && (result_div_01 > 0);
        const auto result_div_02_is_ok = isinf(result_div_02) && (result_div_02 < 0);

        const auto result_cmp_is_ok = (result_div_01_is_ok && result_div_02_is_ok);

        BOOST_TEST(result_div_01_is_ok);
        BOOST_TEST(result_div_02_is_ok);

        result_is_ok = (result_cmp_is_ok && result_is_ok);
      }

      {
        const auto result_div_01 = ( local_one / local_inf_rhs);
        const auto result_div_02 = (-local_one / local_inf_rhs);

        const auto result_div_01_is_ok = (fpclassify(result_div_01) == FP_ZERO) && (!signbit(result_div_01));
        const auto result_div_02_is_ok = (fpclassify(result_div_02) == FP_ZERO) && signbit(result_div_02);

        const auto result_cmp_is_ok = (result_div_01_is_ok && result_div_02_is_ok);

        BOOST_TEST(result_div_01_is_ok);
        BOOST_TEST(result_div_02_is_ok);

        result_is_ok = (result_cmp_is_ok && result_is_ok);
      }

      {
        const auto result_div_01 = (( ::my_nan() * static_cast<decimal_type>(dist(gen))) / local_one);
        const auto result_div_02 = ((-::my_nan() * static_cast<decimal_type>(dist(gen))) / local_one);

        const auto result_div_01_is_ok = isnan(result_div_01);
        const auto result_div_02_is_ok = isnan(result_div_02);

        const auto result_cmp_is_ok = (result_div_01_is_ok && result_div_02_is_ok);

        BOOST_TEST(result_div_01_is_ok);
        BOOST_TEST(result_div_02_is_ok);

        result_is_ok = (result_cmp_is_ok && result_is_ok);
      }

      {
        const auto result_div_01 = ((::my_zero() * static_cast<decimal_type>(dist(gen))) /  local_one);
        const auto result_div_02 = ((::my_zero() * static_cast<decimal_type>(dist(gen))) / -local_one);

        const auto result_div_01_is_ok = ((fpclassify(result_div_01) == FP_ZERO) && (!signbit(result_div_01)));
        const auto result_div_02_is_ok = ((fpclassify(result_div_02) == FP_ZERO) &&   signbit(result_div_02));

        const auto result_cmp_is_ok = (result_div_01_is_ok && result_div_02_is_ok);

        BOOST_TEST(result_cmp_is_ok);

        result_is_ok = (result_cmp_is_ok && result_is_ok);
      }
    }

    {
      decimal_type big {2, 0};

      for(auto   index = static_cast<std::size_t>(UINT8_C(0));
                 index < static_cast<std::size_t>(UINT16_C(1000));
               ++index)
      {
        static_cast<void>(index);

        big *= big;
      }

      const auto result_big_inf_is_ok = isinf(big);

      BOOST_TEST(result_big_inf_is_ok);

      result_is_ok = (result_big_inf_is_ok && result_is_ok);
    }

    return result_is_ok;
  }

  auto test_edges() -> bool
  {
    using decimal_type = boost::decimal::decimal32_t;

    auto result_is_ok = true;

    {
      const decimal_type a = my_a();
      const decimal_type b = my_b();

      const decimal_type c = a + b;

      const auto result_prec_add_is_ok = (c == static_cast<decimal_type>(123456.8));

      BOOST_TEST(result_prec_add_is_ok);

      result_is_ok = (result_prec_add_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(128)); ++i)
    {
      static_cast<void>(i);

      const decimal_type a = my_a() * static_cast<decimal_type>(dist(gen));
      const decimal_type b = my_b() * static_cast<decimal_type>(dist(gen));

      const decimal_type c = a + b;

      const auto result_prec_add_vary_is_ok = (c > decimal_type(123456.709876543));

      BOOST_TEST(result_prec_add_vary_is_ok);

      result_is_ok = (result_prec_add_vary_is_ok && result_is_ok);
    }

    {
      for(auto   index = static_cast<unsigned>(UINT8_C(0));
                 index < static_cast<unsigned>(UINT8_C(128));
               ++index)
      {
        auto dis_lhs =
          std::uniform_real_distribution<float>
          {
            static_cast<float>(1.0E-5L),
            static_cast<float>(2.0E+5L)
          };

        auto dis_rhs =
          std::uniform_real_distribution<float>
          {
            static_cast<float>(8.0E-2L),
            static_cast<float>(11.0E-2L)
          };

        const auto lhs_flt = dis_lhs(gen);
        const auto rhs_flt = dis_rhs(gen);

        const auto lhs_dec = decimal_type(lhs_flt);
        const auto rhs_dec = decimal_type(rhs_flt);

        const auto sum_dec = lhs_dec + rhs_dec;
        const auto sum_flt = lhs_flt + rhs_flt;

        const auto result_sum_is_ok = is_close_fraction(sum_flt, static_cast<float>(sum_dec), std::numeric_limits<float>::epsilon() * 16);

        BOOST_TEST(result_sum_is_ok);

        result_is_ok = (result_sum_is_ok && result_is_ok);
      }
    }

    {
      const auto arg_tiny = decimal_type { std::numeric_limits<decimal_type>::epsilon() } / 1000U;

      const auto sin_tiny = sin(arg_tiny);

      const auto result_sin_tiny_is_ok = (sin_tiny == arg_tiny);

      const auto cos_tiny = cos(arg_tiny);

      const auto result_cos_tiny_is_ok = (cos_tiny == 1);

      const auto result_sin_cos_tiny_is_ok = (result_sin_tiny_is_ok && result_cos_tiny_is_ok);

      BOOST_TEST(result_sin_cos_tiny_is_ok);

      result_is_ok = (result_sin_cos_tiny_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto sin_inf = sin(std::numeric_limits<decimal_type>::infinity() * static_cast<decimal_type>(dist(gen)));
      const auto sin_nan = sin(std::numeric_limits<decimal_type>::quiet_NaN() * static_cast<decimal_type>(dist(gen)));

      const auto cos_inf = cos(std::numeric_limits<decimal_type>::infinity() * static_cast<decimal_type>(dist(gen)));
      const auto cos_nan = cos(std::numeric_limits<decimal_type>::quiet_NaN() * static_cast<decimal_type>(dist(gen)));

      const auto result_sin_cos_non_normal_is_ok =
        (
             (isinf(sin_inf) && isnan(sin_nan))
          && (isinf(cos_inf) && isnan(cos_nan))
        );

      BOOST_TEST(result_sin_cos_non_normal_is_ok);

      result_is_ok = (result_sin_cos_non_normal_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto ilogb_inf_inline   = ilogb(std::numeric_limits<decimal_type>::infinity() * static_cast<decimal_type>(dist(gen)));
      const auto ilogb_inf_callable = ilogb(::my_inf() * static_cast<decimal_type>(dist(gen)));

      const volatile auto result_ilogb_inf_is_ok = ((ilogb_inf_inline == INT_MAX) && (ilogb_inf_callable == INT_MAX));

      BOOST_TEST(result_ilogb_inf_is_ok);

      result_is_ok = (result_ilogb_inf_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto ceil_pi_pos_inline   = ceil(boost::decimal::numbers::pi_v<decimal_type> * static_cast<decimal_type>(dist(gen)));
      const auto ceil_pi_neg_inline   = ceil(-boost::decimal::numbers::pi_v<decimal_type> * static_cast<decimal_type>(dist(gen)));
      const auto ceil_pi_pos_callable = ceil(2 * ::my_pi() * static_cast<decimal_type>(dist(gen)));
      const auto ceil_pi_neg_callable = ceil(-2 * ::my_pi() * static_cast<decimal_type>(dist(gen)));

      const volatile auto result_ceil_is_ok =
        (
             ((ceil_pi_pos_inline   == 4) && (ceil_pi_neg_inline   == -3))
          && ((ceil_pi_pos_callable == 7) && (ceil_pi_neg_callable == -6))
        );

      BOOST_TEST(result_ceil_is_ok);

      result_is_ok = (result_ceil_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto ilogb_inf = ilogb(std::numeric_limits<decimal_type>::infinity() * static_cast<decimal_type>(dist(gen)));

      const auto result_ilogb_inf_is_ok = (ilogb_inf == INT_MAX);

      BOOST_TEST(result_ilogb_inf_is_ok);

      result_is_ok = (result_ilogb_inf_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto exp2_val_zero_pos = exp2( my_zero() * static_cast<decimal_type>(dist(gen)));
      const auto exp2_val_zero_neg = exp2(-my_zero());

      const volatile auto result_pos_is_ok = ((fpclassify(exp2_val_zero_pos) == FP_NORMAL) && (exp2_val_zero_pos == ::my_one()));
      const volatile auto result_neg_is_ok = ((fpclassify(exp2_val_zero_neg) == FP_NORMAL) && (exp2_val_zero_neg == ::my_one()));

      BOOST_TEST(result_pos_is_ok);
      BOOST_TEST(result_neg_is_ok);

      result_is_ok = (result_pos_is_ok && result_neg_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto exp2_val_inf_pos = exp2( std::numeric_limits<decimal_type>::infinity() * static_cast<decimal_type>(dist(gen)));
      const auto exp2_val_inf_neg = exp2(-std::numeric_limits<decimal_type>::infinity() * static_cast<decimal_type>(dist(gen)));

      const volatile auto result_pos_is_ok = ((fpclassify(exp2_val_inf_pos) == FP_INFINITE) && (!signbit(exp2_val_inf_pos)));
      const volatile auto result_neg_is_ok = ((fpclassify(exp2_val_inf_neg) == FP_ZERO)     && (!signbit(exp2_val_inf_neg)));

      BOOST_TEST(result_pos_is_ok);
      BOOST_TEST(result_neg_is_ok);

      result_is_ok = (result_pos_is_ok && result_neg_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto exp2_val_nan = exp2(std::numeric_limits<decimal_type>::quiet_NaN() * static_cast<decimal_type>(dist(gen)));

      const volatile auto result_nan_is_ok = isnan(exp2_val_nan);

      BOOST_TEST(result_nan_is_ok);

      result_is_ok = (result_nan_is_ok && result_is_ok);
    }

    return result_is_ok;
  }
}

auto main() -> int
{
  local::gen.seed(local::time_point<typename std::mt19937_64::result_type>());

  auto result_is_ok = (local::test_behave_over_under() && local::test_edges());

  result_is_ok = ((boost::report_errors() == 0) && result_is_ok);

  return (result_is_ok ? 0 : -1);
}

auto my_zero() -> boost::decimal::decimal32_t& { static boost::decimal::decimal32_t val_zero { 0, 0 }; return val_zero; }
auto my_one () -> boost::decimal::decimal32_t& { static boost::decimal::decimal32_t val_one  { 1, 0 }; return val_one; }
auto my_inf () -> boost::decimal::decimal32_t& { static boost::decimal::decimal32_t val_inf = std::numeric_limits<boost::decimal::decimal32_t>::infinity(); return val_inf; }
auto my_nan () -> boost::decimal::decimal32_t& { static boost::decimal::decimal32_t val_nan = std::numeric_limits<boost::decimal::decimal32_t>::quiet_NaN(); return val_nan; }
auto my_pi  () -> boost::decimal::decimal32_t& { static boost::decimal::decimal32_t val_pi  = boost::decimal::numbers::pi_v<boost::decimal::decimal32_t>; return val_pi; }
auto my_a   () -> boost::decimal::decimal32_t& { static boost::decimal::decimal32_t val_a { 1.234567e5 }; return val_a; }
auto my_b   () -> boost::decimal::decimal32_t& { static boost::decimal::decimal32_t val_b { 9.876543e-2 }; return val_b; }
