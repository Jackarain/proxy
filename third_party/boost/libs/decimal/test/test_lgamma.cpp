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

template<typename DecimalType> auto my_zero() -> DecimalType&;
template<typename DecimalType> auto my_one () -> DecimalType&;

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
  auto test_special_issue385(const int tol_factor) -> bool
  {
    using decimal_type = DecimalType;
    using float_type   = FloatType;

    auto result_is_ok = true;

    const auto x_flt = static_cast<float_type>(2.108116045866610L);
    const auto x_dec = static_cast<decimal_type>(x_flt);

    using std::lgamma;

    const auto val_flt = lgamma(x_flt);
    const auto val_dec = lgamma(x_dec);

    const auto result_val_is_ok = is_close_fraction(val_flt, static_cast<float_type>(val_dec), static_cast<float_type>(std::numeric_limits<decimal_type>::epsilon()) * static_cast<float_type>(tol_factor));

    result_is_ok = (result_val_is_ok && result_is_ok);

    if(!result_val_is_ok)
    {
      // LCOV_EXCL_START
      std::cout << "x_flt  : " << std::scientific << std::setprecision(std::numeric_limits<float_type>::digits10) << x_flt   << std::endl;
      std::cout << "val_flt: " << std::scientific << std::setprecision(std::numeric_limits<float_type>::digits10) << val_flt << std::endl;
      std::cout << "val_dec: " << std::scientific << std::setprecision(std::numeric_limits<float_type>::digits10) << val_dec << std::endl;
      // LCOV_EXCL_STOP
    }

    BOOST_TEST(result_is_ok);

    return result_is_ok;
  }

  template<typename DecimalType, typename FloatType>
  auto test_lgamma(const int tol_factor, const double range_lo, const double range_hi) -> bool
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
    constexpr std::uint32_t count { ((std::numeric_limits<DecimalType>::digits10 < 10) ? UINT16_C(800) : UINT16_C(200)) };
    #else
    constexpr std::uint32_t count { ((std::numeric_limits<DecimalType>::digits10 < 10) ? UINT16_C(80)  : UINT16_C(20)) };
    #endif

    for( ; trials < count; ++trials)
    {
      const auto x_flt = dis(gen);
      const auto x_dec = static_cast<decimal_type>(x_flt);

      using std::lgamma;

      const auto val_flt = lgamma(x_flt);
      const auto val_dec = lgamma(x_dec);

      const auto result_val_is_ok = is_close_fraction(val_flt, static_cast<float_type>(val_dec), static_cast<float_type>(std::numeric_limits<decimal_type>::epsilon()) * static_cast<float_type>(tol_factor));

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

  #if !defined(BOOST_DECIMAL_UNSUPPORTED_LONG_DOUBLE)

  auto test_lgamma_neg32(const int tol_factor) -> bool
  {
    // Table[N[Log[Gamma[-23/100 - n]], 32], {n, 1, 12, 1}]
    // See also Godbolt: https://godbolt.org/z/4zvY3be9M

    using ctrl_as_long_double_array_type = std::array<long double, static_cast<std::size_t>(UINT8_C(12))>;

    constexpr auto ctrl_values =
      ctrl_as_long_double_array_type
      {
        +1.4447269693351526224039790879560L,
        +0.64272538386312523180385678760158L,
        -0.52975675337143994041872235279067L,
        -1.9719587464296265419941805870392L,
        -3.6263700245064580747683408545835L,
        -5.4557463573058198501555262293278L,
        -7.4339853934764931689811998755925L,
        -9.5417714081654716128550899316691L,
        -11.764230456680232402202048996379L,
        -14.089555036643767515552419226440L,
        -16.508143805394119328051805784031L,
        -19.012035755093200315277355952189L,
      };

    auto result_is_ok = true;

    using decimal_type = boost::decimal::decimal32_t;
    using float_type = float;

    for(auto   n = static_cast<std::size_t>(UINT8_C(0));
               n < std::tuple_size<ctrl_as_long_double_array_type>::value;
             ++n)
    {
      const auto ld_arg = -0.23L - static_cast<long double>(n + 1U);

      const auto x_dec = static_cast<boost::decimal::decimal32_t>(ld_arg);
      const auto x_flt = static_cast<float>(ld_arg);

      const auto val_flt = static_cast<float>(ctrl_values[n]);
      const auto val_dec = lgamma(x_dec);

      const auto result_val_is_ok = is_close_fraction(val_flt, static_cast<float_type>(val_dec), static_cast<float_type>(std::numeric_limits<decimal_type>::epsilon()) * static_cast<float_type>(tol_factor));

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

  #endif

  template<typename DecimalType, typename FloatType>
  auto test_lgamma_edge() -> bool
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

      const auto val_nan = lgamma(std::numeric_limits<decimal_type>::quiet_NaN() * static_cast<decimal_type>(dist(gen)));

      const auto result_val_nan_is_ok = isnan(val_nan);

      BOOST_TEST(result_val_nan_is_ok);

      result_is_ok = (result_val_nan_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_inf_pos = lgamma(std::numeric_limits<decimal_type>::infinity() * static_cast<decimal_type>(dist(gen)));

      const auto result_val_inf_pos_is_ok = isinf(val_inf_pos);

      BOOST_TEST(result_val_inf_pos_is_ok);

      result_is_ok = (result_val_inf_pos_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_inf_neg = lgamma(-std::numeric_limits<decimal_type>::infinity() * static_cast<decimal_type>(dist(gen)));

      const auto result_val_inf_neg_is_ok = isinf(val_inf_neg);

      BOOST_TEST(result_val_inf_neg_is_ok);

      result_is_ok = (result_val_inf_neg_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_zero_pos = lgamma(::my_zero<decimal_type>() * static_cast<decimal_type>(dist(gen)));

      const auto result_val_zero_pos_is_ok = (isinf(val_zero_pos) && (!signbit(val_zero_pos)));

      BOOST_TEST(result_val_zero_pos_is_ok);

      result_is_ok = (result_val_zero_pos_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_zero_neg = lgamma(-::my_zero<decimal_type>() * static_cast<decimal_type>(dist(gen)));

      const auto result_val_zero_neg_is_ok = (isinf(val_zero_neg) && (!signbit(val_zero_neg)));

      BOOST_TEST(result_val_zero_neg_is_ok);

      result_is_ok = (result_val_zero_neg_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(6)); ++i)
    {
      const auto n_neg = -static_cast<int>(i) - static_cast<int>(::my_one<decimal_type>());

      const auto val_neg_int = lgamma( decimal_type { n_neg, 0 } );

      const auto result_val_neg_int_is_ok = isinf(val_neg_int);

      BOOST_TEST(result_val_neg_int_is_ok);

      result_is_ok = (result_val_neg_int_is_ok && result_is_ok);
    }

    for(auto trials = static_cast<unsigned>(UINT8_C(1)); trials <= static_cast<unsigned>(UINT8_C(3)); ++trials)
    {
      static_cast<void>(trials);

      for(auto i = static_cast<unsigned>(UINT8_C(1)); i <= static_cast<unsigned>(UINT8_C(2)); ++i)
      {
        const int n_arg { static_cast<int>(decimal_type { i } + static_cast<decimal_type>(dist(gen) / 10.0F)) };

        const decimal_type arg_one_or_two { n_arg, 0 };

        const auto val_one_or_two = lgamma(arg_one_or_two);

        const auto result_val_one_or_two_is_ok =
        (
             (fpclassify(val_one_or_two) == FP_ZERO)
          &&  is_close_fraction(val_one_or_two, (::my_zero<decimal_type>() * static_cast<decimal_type>(dist(gen))), decimal_type { 5, -1 })
        );

        BOOST_TEST(result_val_one_or_two_is_ok);

        result_is_ok = (result_val_one_or_two_is_ok && result_is_ok);
      }
    }

    return result_is_ok;
  }

  auto test_lgamma_128(const int tol_factor) -> bool
  {
    using decimal_type = boost::decimal::decimal128_t;

    using str_ctrl_array_type = std::array<const char*, 21U>;

    const str_ctrl_array_type ctrl_strings =
    {{
       // Table[N[Log[Gamma[(100 n + 10 n + 1)/100]], 36], {n, 0, 20, 1}]
       "4.59947987804202172251394541100874809",
       "-0.0540386340818523935917550731681660590",
       "0.102418994503958632699253052937769400",
       "0.997464457272922372053206167365619618",
       "2.32975308729902926366841147898554568",
       "3.97393485962892204454162289923259731",
       "5.86078226284320941736299492331704683",
       "7.94629710737608673894522918391574878",
       "10.2000180598708079077541397082157801",
       "12.5995970196581001223988397360238569",
       "15.1279348557753769796480417309555140",
       "17.7715247207270252174494824518277843",
       "20.5194267289921636545853277538118495",
       "23.3625991972628192905542283017434866",
       "26.2934437604612886905683626700964367",
       "29.3054851520909703460851836031022652",
       "32.3931392288932864648032352458271232",
       "35.5515407902467593660096806067295711",
       "38.7764130861225208432040187016879672",
       "42.0639671128620105436453477946728445",
       "45.4108226536777051814945280596645578",
    }};

    std::array<decimal_type, std::tuple_size<str_ctrl_array_type>::value> tg_values   { };
    std::array<decimal_type, std::tuple_size<str_ctrl_array_type>::value> ctrl_values { };

    int nx { 0 };

    bool result_is_ok { true };

    const decimal_type my_tol { std::numeric_limits<decimal_type>::epsilon() * static_cast<decimal_type>(tol_factor) };

    for(auto i = static_cast<std::size_t>(UINT8_C(0)); i < std::tuple_size<str_ctrl_array_type>::value; ++i)
    {
      const decimal_type x_arg =
        decimal_type
        {
            decimal_type { 1, 2 } * nx
          + decimal_type { 1, 1 } * nx
          + 1
        }
        / decimal_type { 1, 2 };

      ++nx;

      tg_values[i] = lgamma(x_arg);

      static_cast<void>
      (
        from_chars(ctrl_strings[i], ctrl_strings[i] + std::strlen(ctrl_strings[i]), ctrl_values[i])
      );

      const auto result_lgamma_is_ok = is_close_fraction(tg_values[i], ctrl_values[i], my_tol);

      result_is_ok = (result_lgamma_is_ok && result_is_ok);
    }

    return result_is_ok;
  }

} // namespace local

auto main() -> int
{
  auto result_is_ok = true;

  {
    using decimal_type = boost::decimal::decimal64_t;
    using float_type   = double;

    const auto result_special_issue385_is_ok = local::test_special_issue385<decimal_type, float_type>(512);

    BOOST_TEST(result_special_issue385_is_ok);

    result_is_ok = (result_special_issue385_is_ok && result_is_ok);
  }

  {
    using decimal_type = boost::decimal::decimal32_t;
    using float_type   = float;

    const auto result_lgamma_is_ok   = local::test_lgamma<decimal_type, float_type>(128, 0.01, 0.9);

    BOOST_TEST(result_lgamma_is_ok);

    result_is_ok = (result_lgamma_is_ok && result_is_ok);
  }

  {
    using decimal_type = boost::decimal::decimal32_t;
    using float_type   = float;

    const auto result_lgamma_is_ok   = local::test_lgamma<decimal_type, float_type>(128, 1.1, 1.9);

    BOOST_TEST(result_lgamma_is_ok);

    result_is_ok = (result_lgamma_is_ok && result_is_ok);
  }

  {
    using decimal_type = boost::decimal::decimal32_t;
    using float_type   = float;

    const auto result_lgamma_is_ok   = local::test_lgamma<decimal_type, float_type>(128, 2.1, 123.4);

    BOOST_TEST(result_lgamma_is_ok);

    result_is_ok = (result_lgamma_is_ok && result_is_ok);
  }

  {
    using decimal_type = boost::decimal::decimal64_t;
    using float_type   = double;

    const auto result_lgamma_is_ok = local::test_lgamma<decimal_type, float_type>(512, 0.01, 0.9);

    BOOST_TEST(result_lgamma_is_ok);

    result_is_ok = (result_lgamma_is_ok && result_is_ok);
  }

  {
    using decimal_type = boost::decimal::decimal64_t;
    using float_type   = double;

    const auto result_lgamma_is_ok = local::test_lgamma<decimal_type, float_type>(512, 1.1, 1.9);

    BOOST_TEST(result_lgamma_is_ok);

    result_is_ok = (result_lgamma_is_ok && result_is_ok);
  }

  {
    using decimal_type = boost::decimal::decimal64_t;
    using float_type   = double;

    const auto result_lgamma_is_ok = local::test_lgamma<decimal_type, float_type>(512, 2.1, 123.4);

    BOOST_TEST(result_lgamma_is_ok);

    result_is_ok = (result_lgamma_is_ok && result_is_ok);
  }

  #if !defined(BOOST_DECIMAL_UNSUPPORTED_LONG_DOUBLE)
  {
    const auto result_neg32_is_ok = local::test_lgamma_neg32(512);

    BOOST_TEST(result_neg32_is_ok);

    result_is_ok = (result_neg32_is_ok && result_is_ok);
  }
  #endif

  {
    using decimal_type = boost::decimal::decimal32_t;
    using float_type   = float;

    const auto result_edge_is_ok = local::test_lgamma_edge<decimal_type, float_type>();

    BOOST_TEST(result_edge_is_ok);

    result_is_ok = (result_edge_is_ok && result_is_ok);
  }

  {
    #ifndef BOOST_DECIMAL_UNSUPPORTED_LONG_DOUBLE
    const auto result_lgamma128_is_ok   = local::test_lgamma_128(2048);

    BOOST_TEST(result_lgamma128_is_ok);

    result_is_ok = (result_lgamma128_is_ok && result_is_ok);
    #endif
  }

  result_is_ok = ((boost::report_errors() == 0) && result_is_ok);

  return (result_is_ok ? 0 : -1);
}

template<typename DecimalType> auto my_zero() -> DecimalType& { using decimal_type = DecimalType; static decimal_type val_zero { 0 }; return val_zero; }
template<typename DecimalType> auto my_one () -> DecimalType& { using decimal_type = DecimalType; static decimal_type val_one  { 1 }; return val_one; }
