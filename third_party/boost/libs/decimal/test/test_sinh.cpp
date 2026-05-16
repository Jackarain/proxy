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

  auto test_sinh(const int tol_factor, const bool negate, const long double range_lo, const long double range_hi) -> bool
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

      using std::sinh;

      const auto val_flt = sinh(x_flt);
      const auto val_dec = sinh(x_dec);

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

  auto test_sinh_edge() -> bool
  {
    using decimal_type = boost::decimal::decimal32_t;

    std::mt19937_64 gen;

    std::uniform_real_distribution<float> dist(1.01F, 1.04F);

    auto result_is_ok = true;

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_nan = sinh(std::numeric_limits<decimal_type>::quiet_NaN() * static_cast<decimal_type>(dist(gen)));

      const auto result_val_nan_is_ok = isnan(val_nan);

      BOOST_TEST(result_val_nan_is_ok);

      result_is_ok = (result_val_nan_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_inf_pos = sinh(std::numeric_limits<decimal_type>::infinity() * static_cast<decimal_type>(dist(gen)));

      const auto result_val_inf_pos_is_ok = isinf(val_inf_pos);

      BOOST_TEST(result_val_inf_pos_is_ok);

      result_is_ok = (result_val_inf_pos_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_inf_neg = sinh(-std::numeric_limits<decimal_type>::infinity() * static_cast<decimal_type>(dist(gen)));

      const auto result_val_inf_neg_is_ok = (isinf(val_inf_neg) && signbit(val_inf_neg));

      BOOST_TEST(result_val_inf_neg_is_ok);

      result_is_ok = (result_val_inf_neg_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_zero_pos = sinh(::my_zero());

      const auto result_val_zero_pos_is_ok = (val_zero_pos == ::my_zero());

      BOOST_TEST(result_val_zero_pos_is_ok);

      result_is_ok = (result_val_zero_pos_is_ok && result_is_ok);
    }

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
      static_cast<void>(i);

      const auto val_zero_neg = sinh(-::my_zero());

      const auto result_val_zero_neg_is_ok = (-val_zero_neg == ::my_zero());

      BOOST_TEST(result_val_zero_neg_is_ok);

      result_is_ok = (result_val_zero_neg_is_ok && result_is_ok);
    }

    return result_is_ok;
  }

  auto test_sinh_64(const int tol_factor) -> bool
  {
    using decimal_type = boost::decimal::decimal64_t;

    using val_ctrl_array_type = std::array<double, 19U>;

    const val_ctrl_array_type ctrl_values =
    {{
      // Table[N[Sinh[n/10 + n/100], 17], {n, 1, 19, 1}]
      0.11022196758117152, 0.22177896631245117, 0.33602219751592705,
      0.45433539871409734, 0.57815160374345427, 0.70897049995516614,
      0.84837659273684347, 0.99805839736781424, 1.1598288906636083,
      1.3356474701241768,  1.5276436865595682,  1.7381430376475061,
      1.9696951348397458,  2.2251045847805740,  2.5074649592795473,
      2.8201962652897691,  3.1670863687357898,  3.5523368739250597,
      3.9806140142438027
    }};

    std::array<decimal_type, std::tuple_size<val_ctrl_array_type>::value> sinh_values { };

    int nx { 1 };

    bool result_is_ok { true };

    const decimal_type my_tol { std::numeric_limits<decimal_type>::epsilon() * static_cast<decimal_type>(tol_factor) };

    for(auto i = static_cast<std::size_t>(UINT8_C(0)); i < std::tuple_size<val_ctrl_array_type>::value; ++i)
    {
      // Table[N[Sinh[n/10 + n/100], 17], {n, 1, 19, 1}]

      const decimal_type
        x_arg
        {
            decimal_type { nx, -1 }
          + decimal_type { nx, -2 }
        };

      sinh_values[i] = sinh(x_arg);

      ++nx;

      const auto result_sinh_is_ok = is_close_fraction(sinh_values[i], decimal_type(ctrl_values[i]), my_tol);

      result_is_ok = (result_sinh_is_ok && result_is_ok);
    }

    return result_is_ok;
  }

  auto test_sinh_128(const int tol_factor) -> bool
  {
    using decimal_type = boost::decimal::decimal128_t;

    using str_ctrl_array_type = std::array<const char*, 19U>;

    const str_ctrl_array_type ctrl_strings =
    {{
       // Table[N[Sinh[n/10 + n/100], 36], {n, 1, 19, 1}]
       "0.110221967581171520411941593088236059",
       "0.221778966312451168268004699999644624",
       "0.336022197515927048343664664421348663",
       "0.454335398714097341758947808091367454",
       "0.578151603743454270751273401127877909",
       "0.708970499955166142056110066313962238",
       "0.848376592736843467566140720904565722",
       "0.998058397367814241955840599316835579",
       "1.15982889066360829928417764208140629",
       "1.33564747012417677938478052357867844",
       "1.52764368655956815153599423849987448",
       "1.73814303764750609928767015311103557",
       "1.96969513483974581353076445747887002",
       "2.22510458478057397430261017728023479",
       "2.50746495927954731170938115076950165",
       "2.82019626528976906072890033791187430",
       "3.16708636873578984466547274878757638",
       "3.55233687392505969901746060667469125",
       "3.98061401424380267930475594525594567",
    }};

    std::array<decimal_type, std::tuple_size<str_ctrl_array_type>::value> sinh_values { };
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

      sinh_values[i] = sinh(x_arg);

      static_cast<void>
      (
        from_chars(ctrl_strings[i], ctrl_strings[i] + std::strlen(ctrl_strings[i]), ctrl_values[i])
      );

      const auto result_sinh_is_ok = is_close_fraction(sinh_values[i], ctrl_values[i], my_tol);

      result_is_ok = (result_sinh_is_ok && result_is_ok);
    }

    return result_is_ok;
  }

} // namespace local

auto main() -> int
{
  auto result_is_ok = true;

  const auto result_pos_is_ok = local::test_sinh(128, false, 0.03125L, 32.0L);
  const auto result_neg_is_ok = local::test_sinh(128, true,  0.03125L, 32.0L);

  const auto result_pos_narrow_is_ok = local::test_sinh(24, false, 0.125L, 8.0L);
  const auto result_neg_narrow_is_ok = local::test_sinh(24, true,  0.125L, 8.0L);

  const auto result_pos_wide_is_ok = local::test_sinh(128, false, 0.015625L, 64.0L);
  const auto result_neg_wide_is_ok = local::test_sinh(128, true,  0.015625L, 64.0L);

  const auto result_edge_is_ok = local::test_sinh_edge();

  const auto result_pos64_is_ok = local::test_sinh_64(64);

  const auto result_pos128_is_ok = local::test_sinh_128(400000);

  BOOST_TEST(result_pos_is_ok);
  BOOST_TEST(result_neg_is_ok);

  BOOST_TEST(result_pos_narrow_is_ok);
  BOOST_TEST(result_neg_narrow_is_ok);

  BOOST_TEST(result_pos_wide_is_ok);
  BOOST_TEST(result_neg_wide_is_ok);

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
