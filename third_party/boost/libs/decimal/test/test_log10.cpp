// Copyright 2024 Matt Borland
// Copyright 2024 Christopher Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include "testing_config.hpp"
#include <boost/decimal.hpp>

#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wfloat-equal"
#elif defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#endif

#include <boost/core/lightweight_test.hpp>

#include <chrono>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <string>

template<typename DecimalType> auto my_zero() -> DecimalType&;
template<typename DecimalType> auto my_one () -> DecimalType&;
template<typename DecimalType> auto my_inf () -> DecimalType&;

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
  auto test_log10(const int tol_factor) -> bool
  {
    using decimal_type = DecimalType;
    using float_type   = FloatType;

    std::random_device rd;
    std::mt19937_64 gen(rd());

    gen.seed(time_point<typename std::mt19937_64::result_type>());

    auto dis_r =
      std::uniform_real_distribution<float_type>
      {
        static_cast<float_type>(1.4L),
        static_cast<float_type>(8.9L)
      };

    bool result_is_ok { true };

    auto trials = static_cast<std::uint32_t>(UINT8_C(0));

    #if !defined(BOOST_DECIMAL_REDUCE_TEST_DEPTH)
    constexpr auto count = (sizeof(decimal_type) == static_cast<std::size_t>(UINT8_C(4))) ? UINT32_C(0x200) : UINT32_C(0x40);
    #else
    constexpr auto count = (sizeof(decimal_type) == static_cast<std::size_t>(UINT8_C(4))) ? UINT32_C(0x40) : UINT32_C(0x4);
    #endif

    for( ; trials < count; ++trials)
    {
      auto x_flt = dis_r(gen);

      auto dis_n =
        std::uniform_int_distribution<int>
        {
          -17,
          17
        };

      std::string str_e { "1.0E" + std::to_string(dis_n(gen)) };

      char* p_end;

      x_flt *= static_cast<float>(strtold(str_e.c_str(), &p_end));

      static_cast<void>(p_end);

      const auto x_dec = static_cast<decimal_type>(x_flt);

      using std::log10;

      const auto val_flt = log10(x_flt);
      const auto val_dec = log10(x_dec);

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
  auto test_log10_pow10() -> bool
  {
    using decimal_type = DecimalType;
    using float_type   = FloatType;

    bool result_is_ok { true };

    for(int i = INT8_C(-23); i <= INT8_C(23); ++i)
    {
      const decimal_type x_arg { 1, i };

      const decimal_type val_dec  = log10(x_arg);
      const float_type   val_ctrl =  static_cast<float_type>(i);

      const float_type val_to_check = static_cast<float_type>(val_dec);

      const auto result_log10_pow10_is_ok = (val_to_check == val_ctrl);

      result_is_ok = (result_log10_pow10_is_ok && result_is_ok);
    }

    BOOST_TEST(result_is_ok);

    return result_is_ok;
  }

  template<typename DecimalType, typename FloatType>
  auto test_log10_edge() -> bool
  {
    using decimal_type = DecimalType;
    using float_type   = FloatType;

    std::mt19937_64 gen;

    gen.seed(time_point<typename std::mt19937_64::result_type>());

    std::uniform_real_distribution<float_type>
      dist
      (
        static_cast<float_type>(1.01L),
        static_cast<float_type>(1.04L)
      );

    volatile auto result_is_ok = true;

    for(auto index = static_cast<unsigned>(UINT8_C(0)); index < static_cast<unsigned>(UINT8_C(4)); ++index)
    {
      static_cast<void>(index);

      decimal_type arg_zero { ::my_zero<decimal_type>() };
      arg_zero *= static_cast<decimal_type>(dist(gen));

      const auto log_zero = log10(arg_zero);

      const volatile auto result_log_zero_is_ok = (isinf(log_zero) && signbit(log_zero));

      BOOST_TEST(result_log_zero_is_ok);

      result_is_ok = (result_log_zero_is_ok && result_is_ok);
    }

    for(auto index = static_cast<unsigned>(UINT8_C(0)); index < static_cast<unsigned>(UINT8_C(4)); ++index)
    {
      static_cast<void>(index);

      decimal_type arg_zero { ::my_zero<decimal_type>() };
      arg_zero *= static_cast<decimal_type>(dist(gen));

      const auto log_zero_minus = log10(-arg_zero);

      const volatile auto result_log_zero_minus_is_ok = (isinf(log_zero_minus) && signbit(log_zero_minus));

      BOOST_TEST(result_log_zero_minus_is_ok);

      result_is_ok = (result_log_zero_minus_is_ok && result_is_ok);
    }

    for(auto index = static_cast<unsigned>(UINT8_C(0)); index < static_cast<unsigned>(UINT8_C(4)); ++index)
    {
      static_cast<void>(index);

      const auto log_one = log10(::my_one<decimal_type>());

      const volatile auto result_log_one_is_ok = (log_one == ::my_zero<decimal_type>() * static_cast<decimal_type>(dist(gen)));

      BOOST_TEST(result_log_one_is_ok);

      result_is_ok = (result_log_one_is_ok && result_is_ok);
    }

    for(auto index = static_cast<unsigned>(UINT8_C(0)); index < static_cast<unsigned>(UINT8_C(4)); ++index)
    {
      static_cast<void>(index);

      const auto log_one_minus = log10(-::my_one<decimal_type>());

      const volatile auto result_log_one_minus_is_ok = isnan(log_one_minus);

      BOOST_TEST(result_log_one_minus_is_ok);

      result_is_ok = (result_log_one_minus_is_ok && result_is_ok);
    }

    for(auto index = static_cast<unsigned>(UINT8_C(0)); index < static_cast<unsigned>(UINT8_C(4)); ++index)
    {
      static_cast<void>(index);

      decimal_type arg_inf { ::my_inf<decimal_type>() };
      arg_inf *= static_cast<decimal_type>(dist(gen));

      const auto log_inf = log10(arg_inf);

      const volatile auto result_log_inf_is_ok = isinf(log_inf);

      BOOST_TEST(result_log_inf_is_ok);

      result_is_ok = (result_log_inf_is_ok && result_is_ok);
    }

    for(auto index = static_cast<unsigned>(UINT8_C(0)); index < static_cast<unsigned>(UINT8_C(4)); ++index)
    {
      static_cast<void>(index);

      decimal_type arg_inf { ::my_inf<decimal_type>() };
      arg_inf *= static_cast<decimal_type>(dist(gen));

      const auto log_inf_minus = log10(-arg_inf);

      const volatile auto result_log_inf_minus_is_ok = isnan(log_inf_minus);

      BOOST_TEST(result_log_inf_minus_is_ok);

      result_is_ok = (result_log_inf_minus_is_ok && result_is_ok);
    }

    for(auto index = static_cast<unsigned>(UINT8_C(0)); index < static_cast<unsigned>(UINT8_C(4)); ++index)
    {
      static_cast<void>(index);

      const auto log_nan = log10(std::numeric_limits<decimal_type>::quiet_NaN() * static_cast<decimal_type>(dist(gen)));

      const volatile auto result_log_nan_is_ok = isnan(log_nan);

      BOOST_TEST(result_log_nan_is_ok);

      result_is_ok = (result_log_nan_is_ok && result_is_ok);
    }

    return result_is_ok;
  }

  auto test_log10_128(const int tol_factor) -> bool
  {
    using decimal_type = boost::decimal::decimal128_t;

    using str_ctrl_array_type = std::array<const char*, 28U>;

    const str_ctrl_array_type ctrl_strings =
    {{
       // Table[N[Log[10, (456 + n) 10^n], 36], {n, -3, 24, 1}]
       "-0.343901797987168125835899041407581902",
       "0.657055852857103915316787859478165974",
       "1.65801139665711240470498252181047160",
       "2.65896484266443498447257806318552371",
       "3.65991620006985022235354614522047714",
       "4.66086547800386918934166876025115190",
       "5.66181268553726124042525360409368296",
       "6.66275783168157407408151600697568258",
       "7.66370092538964814507468181848742134",
       "8.66464197555612550397118302781526877",
       "9.66558099101795313567419310843870855",
       "10.6665179805548808681878023418672271",
       "11.6674529528899539217479931086490380",
       "12.6683859166900001674028777302013524",
       "13.6693168805661121630880510897799967",
       "14.6702458530741240342240387539015451",
       "15.6711728427150832648613478878187048",
       "16.6720978579357174644142193994492006",
       "17.6730209071288961740565090331523990",
       "18.6739419986340877759018730687086233",
       "19.6748611407378115671552881244722464",
       "20.6757783416740850605035881844578580",
       "21.6766936096248665711088556863079433",
       "22.6776069527204931496798639423699593",
       "23.6785183790401139202230480981374872",
       "24.6794278966121188802154000548723851",
       "25.6803355134145632200969639669623108",
       "26.6812412373755872181499834821530874",
    }};

    std::array<decimal_type, std::tuple_size<str_ctrl_array_type>::value> log_values { };
    std::array<decimal_type, std::tuple_size<str_ctrl_array_type>::value> ctrl_values { };

    int nx { -3 };

    bool result_is_ok { true };

    const decimal_type my_tol { std::numeric_limits<decimal_type>::epsilon() * static_cast<decimal_type>(tol_factor) };

    for(auto i = static_cast<std::size_t>(UINT8_C(0)); i < std::tuple_size<str_ctrl_array_type>::value; ++i)
    {
      const decimal_type x_arg { (456 + nx), nx };

      ++nx;

      log_values[i] = log10(x_arg);

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

    const auto test_log10_is_ok = local::test_log10<decimal_type, float_type>(128);

    BOOST_TEST(test_log10_is_ok);

    result_is_ok = (test_log10_is_ok && result_is_ok);
  }

  {
    using decimal_type = boost::decimal::decimal_fast32_t;
    using float_type   = float;

    const auto test_log10_is_ok = local::test_log10<decimal_type, float_type>(128);

    BOOST_TEST(test_log10_is_ok);

    result_is_ok = (test_log10_is_ok && result_is_ok);
  }

  {
    using decimal_type = boost::decimal::decimal64_t;
    using float_type   = double;

    const auto test_log10_is_ok = local::test_log10<decimal_type, float_type>(512);

    BOOST_TEST(test_log10_is_ok);

    result_is_ok = (test_log10_is_ok && result_is_ok);
  }

  {
    using decimal_type = boost::decimal::decimal_fast64_t;
    using float_type   = double;

    const auto test_log10_is_ok = local::test_log10<decimal_type, float_type>(512);

    BOOST_TEST(test_log10_is_ok);

    result_is_ok = (test_log10_is_ok && result_is_ok);
  }

  {
    using decimal_type = boost::decimal::decimal32_t;
    using float_type   = float;

    const auto test_log10_pow10_is_ok = local::test_log10_pow10<decimal_type, float_type>();

    BOOST_TEST(test_log10_pow10_is_ok);

    result_is_ok = (test_log10_pow10_is_ok && result_is_ok);
  }

  {
    using decimal_type = boost::decimal::decimal32_t;
    using float_type   = float;

    const auto test_log10_edge_is_ok = local::test_log10_edge<decimal_type, float_type>();

    BOOST_TEST(test_log10_edge_is_ok);

    result_is_ok = (test_log10_edge_is_ok && result_is_ok);
  }

  {
    using decimal_type = boost::decimal::decimal64_t;
    using float_type   = double;

    const auto test_log10_edge_is_ok = local::test_log10_edge<decimal_type, float_type>();

    BOOST_TEST(test_log10_edge_is_ok);

    result_is_ok = (test_log10_edge_is_ok && result_is_ok);
  }

  {
    const auto result_pos128_is_ok = local::test_log10_128(8192);

    BOOST_TEST(result_pos128_is_ok);

    result_is_ok = (result_pos128_is_ok && result_is_ok);
  }

  result_is_ok = ((boost::report_errors() == 0) && result_is_ok);

  return (result_is_ok ? 0 : -1);
}

template<typename DecimalType> auto my_zero() -> DecimalType& { using decimal_type = DecimalType; static decimal_type val_zero { 0 }; return val_zero; }
template<typename DecimalType> auto my_one () -> DecimalType& { using decimal_type = DecimalType; static decimal_type val_one  { 1 }; return val_one; }
template<typename DecimalType> auto my_inf () -> DecimalType& { using decimal_type = DecimalType; static decimal_type val_inf  { std::numeric_limits<decimal_type>::infinity() }; return val_inf; }
