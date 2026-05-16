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

    if (!result_is_ok)
    // LCOV_EXCL_START
    {
      std::stringstream strm;

      strm << std::setprecision(std::numeric_limits<NumericType>::digits10)
           <<   "a    : " << a
           << "\nb    : " << b
           << "\ndelta: " << delta
           << "\ntol  : " << tol;

      std::cerr << strm.str() << std::endl;
    }
    // LCOV_EXCL_STOP

    return result_is_ok;
  }

  template<typename DecimalType, typename FloatType>
  auto test_log(const int tol_factor) -> bool
  {
    using decimal_type = DecimalType;
    using float_type   = FloatType;

    std::random_device rd;
    std::mt19937_64 gen(rd());

    gen.seed(time_point<typename std::mt19937_64::result_type>());

    auto dis =
      std::uniform_real_distribution<float_type>
      {
        static_cast<float_type>(1.0E-17L),
        static_cast<float_type>(1.0E+17L)
      };

    auto result_is_ok = true;

    auto trials = static_cast<std::uint32_t>(UINT8_C(0));

    #if !defined(BOOST_DECIMAL_REDUCE_TEST_DEPTH)
    constexpr auto count = (sizeof(decimal_type) == static_cast<std::size_t>(UINT8_C(4))) ? UINT32_C(0x200) : UINT32_C(0x40);
    #else
    constexpr auto count = (sizeof(decimal_type) == static_cast<std::size_t>(UINT8_C(4))) ? UINT32_C(0x40) : UINT32_C(0x4);
    #endif

    for( ; trials < count; ++trials)
    {
      const auto x_flt = dis(gen);
      const auto x_dec = static_cast<decimal_type>(x_flt);

      using std::log;

      const auto val_flt = log(x_flt);
      const auto val_dec = log(x_dec);

      const auto result_log_is_ok = is_close_fraction(val_flt, static_cast<float_type>(val_dec), std::numeric_limits<float_type>::epsilon() * static_cast<float_type>(tol_factor));

      result_is_ok = (result_log_is_ok && result_is_ok);

      if(!result_log_is_ok)
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
  auto test_log_between_1_and_2(const int tol_factor) -> bool
  {
    using decimal_type = DecimalType;
    using float_type   = FloatType;

    auto result_is_ok = true;

    for(auto   ui_arg = static_cast<unsigned>(UINT8_C(106));
               ui_arg < static_cast<unsigned>(UINT8_C(205));
             ++ui_arg)
    {
      const auto x_dec = static_cast<decimal_type>(ui_arg) / 100U;
      const auto x_flt = static_cast<float_type>(x_dec);

      using std::log;

      const auto val_flt = log(x_flt);
      const auto val_dec = log(x_dec);

      const auto result_log_is_ok = is_close_fraction(val_flt, static_cast<float_type>(val_dec), std::numeric_limits<float_type>::epsilon() * static_cast<float_type>(tol_factor));

      result_is_ok = (result_log_is_ok && result_is_ok);

      if(!result_log_is_ok)
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
  auto test_log_edge(const int tol_factor) -> bool
  {
    using decimal_type = DecimalType;
    using float_type   = FloatType;

    std::mt19937_64 gen;

    gen.seed(time_point<typename std::mt19937_64::result_type>());

    std::uniform_real_distribution<float_type>
      dist
      {
        static_cast<float_type>(1.01L),
        static_cast<float_type>(1.04L)
      };

    volatile auto result_is_ok = true;

    for(auto index = static_cast<unsigned>(UINT8_C(0)); index < static_cast<unsigned>(UINT8_C(8)); ++index)
    {
      static_cast<void>(index);

      const decimal_type arg_zero = ::my_zero<decimal_type>() * static_cast<decimal_type>(dist(gen));

      const auto log_zero = log(arg_zero);

      const volatile bool result_log_zero_is_ok
      {
           (fpclassify(arg_zero) == FP_ZERO)
        && isinf(log_zero)
        && signbit(log_zero)
      };

      BOOST_TEST(result_log_zero_is_ok);

      result_is_ok = (result_log_zero_is_ok && result_is_ok);
    }

    for(auto index = static_cast<unsigned>(UINT8_C(0)); index < static_cast<unsigned>(UINT8_C(8)); ++index)
    {
      static_cast<void>(index);

      const decimal_type arg_zero_minus = -::my_zero<decimal_type>() * static_cast<decimal_type>(dist(gen));

      const auto log_zero_minus = log(arg_zero_minus);

      const volatile auto result_log_zero_minus_is_ok = (isinf(log_zero_minus) && signbit(log_zero_minus));

      BOOST_TEST(result_log_zero_minus_is_ok);

      result_is_ok = (result_log_zero_minus_is_ok && result_is_ok);
    }

    for(auto index = static_cast<unsigned>(UINT8_C(0)); index < static_cast<unsigned>(UINT8_C(8)); ++index)
    {
      static_cast<void>(index);

      decimal_type arg_one =
        static_cast<decimal_type>
        (
          static_cast<int>(::my_one<decimal_type>() * static_cast<decimal_type>(dist(gen)))
        );

      const auto log_one = log(arg_one);

      const volatile auto result_log_one_is_ok = ((arg_one == ::my_one<decimal_type>()) && (log_one == ::my_zero<decimal_type>()));

      BOOST_TEST(result_log_one_is_ok);

      result_is_ok = (result_log_one_is_ok && result_is_ok);
    }

    for(auto index = static_cast<unsigned>(UINT8_C(0)); index < static_cast<unsigned>(UINT8_C(8)); ++index)
    {
      static_cast<void>(index);

      decimal_type arg_one_minus =
        static_cast<decimal_type>
        (
          -static_cast<int>(::my_one<decimal_type>() * static_cast<decimal_type>(dist(gen)))
        );

      const auto log_one_minus = log(arg_one_minus);

      const volatile auto result_log_one_minus_is_ok = ((-arg_one_minus == ::my_one<decimal_type>()) &&  isnan(log_one_minus));

      BOOST_TEST(result_log_one_minus_is_ok);

      result_is_ok = (result_log_one_minus_is_ok && result_is_ok);
    }

    for(auto index = static_cast<unsigned>(UINT8_C(0)); index < static_cast<unsigned>(UINT8_C(8)); ++index)
    {
      static_cast<void>(index);

      const auto log_inf = log(::my_inf<decimal_type>() * static_cast<decimal_type>(dist(gen)));

      const volatile auto result_log_inf_is_ok = isinf(log_inf);

      BOOST_TEST(result_log_inf_is_ok);

      result_is_ok = (result_log_inf_is_ok && result_is_ok);
    }

    for(auto index = static_cast<unsigned>(UINT8_C(0)); index < static_cast<unsigned>(UINT8_C(8)); ++index)
    {
      static_cast<void>(index);

      const auto log_inf_minus = log(-::my_inf<decimal_type>() * static_cast<decimal_type>(dist(gen)));

      const volatile auto result_log_inf_minus_is_ok = isnan(log_inf_minus);

      BOOST_TEST(result_log_inf_minus_is_ok);

      result_is_ok = (result_log_inf_minus_is_ok && result_is_ok);
    }

    for(auto index = static_cast<unsigned>(UINT8_C(0)); index < static_cast<unsigned>(UINT8_C(8)); ++index)
    {
      static_cast<void>(index);

      const decimal_type arg_nan = std::numeric_limits<decimal_type>::quiet_NaN() * static_cast<decimal_type>(dist(gen));

      const decimal_type log_nan = log(arg_nan);

      const volatile auto result_log_nan_is_ok = (isnan(arg_nan) && isnan(log_nan));

      BOOST_TEST(result_log_nan_is_ok);

      result_is_ok = (result_log_nan_is_ok && result_is_ok);
    }

    for(auto index = static_cast<unsigned>(UINT8_C(0)); index < static_cast<unsigned>(UINT8_C(8)); ++index)
    {
      const auto x_flt = static_cast<float_type>(1.4L + static_cast<long double>(index) / 20.0L);
      const auto x_dec = static_cast<decimal_type>(x_flt);

      using std::log;

      const auto lg_flt = log(x_flt);
      const auto lg_dec = log(x_dec);

      const auto result_log_is_ok = is_close_fraction(lg_flt, static_cast<float_type>(lg_dec), std::numeric_limits<float_type>::epsilon() * static_cast<float_type>(tol_factor));

      BOOST_TEST(result_log_is_ok);

      result_is_ok = (result_log_is_ok && result_is_ok);
    }

    for(auto index = static_cast<unsigned>(UINT8_C(0)); index < static_cast<unsigned>(UINT8_C(9)); ++index)
    {
      const auto x_flt = static_cast<float_type>(0.1L + static_cast<long double>(index) / 10.0L);
      const auto x_dec = static_cast<decimal_type>(x_flt);

      using std::log;

      const auto lg_flt          = log(x_flt);
      const auto lg_dec          = log(x_dec);
      const auto lg_dec_as_float = static_cast<float_type>(lg_dec);

      const auto result_log_is_ok = is_close_fraction(lg_flt, lg_dec_as_float, std::numeric_limits<float_type>::epsilon() * static_cast<float_type>(tol_factor));

      BOOST_TEST(result_log_is_ok);

      result_is_ok = (result_log_is_ok && result_is_ok);
    }

    return result_is_ok;
  }

  auto test_log_64(const int tol_factor) -> bool
  {
    using decimal_type = boost::decimal::decimal64_t;

    using val_ctrl_array_type = std::array<double, 28U>;

    const val_ctrl_array_type ctrl_values =
    {{
      // Table[N[Log[111 10^n], 17], {n, -3, 24, 1}]
      -2.1982250776698029, 0.10436001532424277, 2.4069451083182885,
       4.7095302013123341, 7.0121152943063798, 9.3147003873004255,
       11.617285480294471, 13.919870573288517, 16.222455666282563,
       18.525040759276608, 20.827625852270654, 23.130210945264700,
       25.432796038258745, 27.735381131252791, 30.037966224246837,
       32.340551317240882, 34.643136410234928, 36.945721503228974,
       39.248306596223019, 41.550891689217065, 43.853476782211111,
       46.156061875205156, 48.458646968199202, 50.761232061193248,
       53.063817154187294, 55.366402247181339, 57.668987340175385,
       59.971572433169431
    }};

    std::array<decimal_type, std::tuple_size<val_ctrl_array_type>::value> log_values { };

    int nx { -3 };

    bool result_is_ok { true };

    const decimal_type my_tol { std::numeric_limits<decimal_type>::epsilon() * static_cast<decimal_type>(tol_factor) };

    for(auto i = static_cast<std::size_t>(UINT8_C(0)); i < std::tuple_size<val_ctrl_array_type>::value; ++i)
    {
      // Table[N[Log[111 10^n], 17], {n, -3, 24, 1}]

      const decimal_type x_arg { 111, nx };

      log_values[i] = log(x_arg);

      ++nx;

      const auto result_log_is_ok = is_close_fraction(log_values[i], decimal_type(ctrl_values[i]), my_tol);

      result_is_ok = (result_log_is_ok && result_is_ok);
    }

    return result_is_ok;
  }

  auto test_log_128(const int tol_factor) -> bool
  {
    using decimal_type = boost::decimal::decimal128_t;

    using str_ctrl_array_type = std::array<const char*, 28U>;

    const str_ctrl_array_type ctrl_strings =
    {{
       // Table[N[Log[111 10^n], 36], {n, -3, 24, 1}]
       "-2.19822507766980291629063345609911975",
       "0.104360015324242767727357998585244453",
       "2.40694510831828845174534945326960866",
       "4.70953020131233413576334090795397287",
       "7.01211529430637981978133236263833708",
       "9.31470038730042550379932381732270128",
       "11.6172854802944711878173152720070655",
       "13.9198705732885168718353067266914297",
       "16.2224556662825625558532981813757939",
       "18.5250407592766082398712896360601581",
       "20.8276258522706539238892810907445223",
       "23.1302109452646996079072725454288865",
       "25.4327960382587452919252640001132507",
       "27.7353811312527909759432554547976149",
       "30.0379662242468366599612469094819792",
       "32.3405513172408823439792383641663434",
       "34.6431364102349280279972298188507076",
       "36.9457215032289737120152212735350718",
       "39.2483065962230193960332127282194360",
       "41.5508916892170650800512041829038002",
       "43.8534767822111107640691956375881644",
       "46.1560618752051564480871870922725286",
       "48.4586469681992021321051785469568928",
       "50.7612320611932478161231700016412570",
       "53.0638171541872935001411614563256212",
       "55.3664022471813391841591529110099854",
       "57.6689873401753848681771443656943496",
       "59.9715724331694305521951358203787139",
    }};

    std::array<decimal_type, std::tuple_size<str_ctrl_array_type>::value> log_values { };
    std::array<decimal_type, std::tuple_size<str_ctrl_array_type>::value> ctrl_values { };

    int nx { -3 };

    bool result_is_ok { true };

    const decimal_type my_tol { std::numeric_limits<decimal_type>::epsilon() * static_cast<decimal_type>(tol_factor) };

    for(auto i = static_cast<std::size_t>(UINT8_C(0)); i < std::tuple_size<str_ctrl_array_type>::value; ++i)
    {
      const decimal_type x_arg { 111, nx };

      ++nx;

      log_values[i] = log(x_arg);

      static_cast<void>
      (
        from_chars(ctrl_strings[i], ctrl_strings[i] + std::strlen(ctrl_strings[i]), ctrl_values[i])
      );

      const auto result_log_is_ok = is_close_fraction(log_values[i], ctrl_values[i], my_tol);

      result_is_ok = (result_log_is_ok && result_is_ok);
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

    const auto test_log_is_ok                 = local::test_log                <decimal_type, float_type>(32);
    const auto test_log_between_1_and_2_is_ok = local::test_log_between_1_and_2<decimal_type, float_type>(64);
    const auto test_log_edge_is_ok            = local::test_log_edge           <decimal_type, float_type>(32);

    result_is_ok = (test_log_is_ok && test_log_between_1_and_2_is_ok && test_log_edge_is_ok && result_is_ok);
  }

  {
    using decimal_type = boost::decimal::decimal64_t;
    using float_type   = double;

    const auto test_log_is_ok                 = local::test_log                <decimal_type, float_type>(64);
    const auto test_log_between_1_and_2_is_ok = local::test_log_between_1_and_2<decimal_type, float_type>(512);
    const auto test_log_edge_is_ok            = local::test_log_edge           <decimal_type, float_type>(64);

    result_is_ok = (test_log_is_ok && test_log_between_1_and_2_is_ok && test_log_edge_is_ok && result_is_ok);
  }

  {
    const auto result_pos64_is_ok = local::test_log_64(256);

    BOOST_TEST(result_pos64_is_ok);

    result_is_ok = (result_pos64_is_ok && result_is_ok);
  }

  {
    const auto result_pos128_is_ok = local::test_log_128(128);

    BOOST_TEST(result_pos128_is_ok);

    result_is_ok = (result_pos128_is_ok && result_is_ok);
  }

  result_is_ok = ((boost::report_errors() == 0) && result_is_ok);

  return (result_is_ok ? 0 : -1);
}

template<typename DecimalType> auto my_zero() -> DecimalType& { using decimal_type = DecimalType; static decimal_type my_zero_val { 0, 0 }; return my_zero_val; }
template<typename DecimalType> auto my_one () -> DecimalType& { using decimal_type = DecimalType; static decimal_type my_one_val  { 1, 0 }; return my_one_val; }
template<typename DecimalType> auto my_inf () -> DecimalType& { using decimal_type = DecimalType; static decimal_type my_inf_val  { std::numeric_limits<decimal_type>::infinity() };  return my_inf_val; }
template<typename DecimalType> auto my_nan () -> DecimalType& { using decimal_type = DecimalType; static decimal_type my_nan_val  { std::numeric_limits<decimal_type>::quiet_NaN() }; return my_nan_val; }
