// Copyright 2024 - 2026 Matt Borland
// Copyright 2024 - 2026 Christoper Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

// Propogates up from boost.math
#define _SILENCE_CXX23_DENORM_DEPRECATION_WARNING

#ifdef _MSC_VER
# pragma warning(push)
# pragma warning(disable: 4714) // Marked as forceinline but not inlined
#endif

#include "testing_config.hpp"
#include <boost/decimal.hpp>

#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wold-style-cast"
#  pragma clang diagnostic ignored "-Wundef"
#  pragma clang diagnostic ignored "-Wconversion"
#  pragma clang diagnostic ignored "-Wsign-conversion"
#  pragma clang diagnostic ignored "-Wfloat-equal"
#  if __clang_major__ >= 20
#    pragma clang diagnostic ignored "-Wfortify-source"
#  endif
#elif defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wold-style-cast"
#  pragma GCC diagnostic ignored "-Wundef"
#  pragma GCC diagnostic ignored "-Wconversion"
#  pragma GCC diagnostic ignored "-Wsign-conversion"
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#  pragma GCC diagnostic ignored "-Wuseless-cast"
#endif

#include <boost/core/lightweight_test.hpp>
#include <boost/math/special_functions/zeta.hpp>

#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>

template<typename DecimalType> auto my_zero() -> DecimalType&;
template<typename DecimalType> auto my_one () -> DecimalType&;
template<typename DecimalType> auto my_nan () -> DecimalType&;
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
  auto test_riemann_zeta(const int tol_factor, const long double range_lo, const long double range_hi) -> bool
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

    bool result_is_ok { true };

    auto trials = static_cast<std::uint32_t>(UINT8_C(0));

    #if !defined(BOOST_DECIMAL_REDUCE_TEST_DEPTH)
    constexpr auto count = (sizeof(decimal_type) == static_cast<std::size_t>(UINT8_C(4))) ? UINT32_C(0x80) : UINT32_C(0x20);
    #else
    constexpr auto count = (sizeof(decimal_type) == static_cast<std::size_t>(UINT8_C(4))) ? UINT32_C(0x10) : UINT32_C(0x4);
    #endif

    for( ; trials < count; ++trials)
    {
      const auto x_flt = dis(gen);
      const auto x_dec = static_cast<decimal_type>(x_flt);

      const auto val_flt = boost::math::zeta(x_flt);
      const auto val_dec = riemann_zeta(x_dec);

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
auto test_riemann_zeta_edge() -> bool
{
  using decimal_type = DecimalType;
  using float_type   = FloatType;

  bool result_is_ok { true };

  std::mt19937_64 gen;

  gen.seed(time_point<typename std::mt19937_64::result_type>());

  std::uniform_real_distribution<float_type> dist(static_cast<float_type>(1.1L), static_cast<float_type>(101.1L));

  for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(10)); ++i)
  {
    static_cast<void>(i);

    {
      const decimal_type inf  { ::my_inf <decimal_type>() * decimal_type(dist(gen)) };

      bool result_inf_is_ok { };

      result_inf_is_ok = (riemann_zeta(inf) == decimal_type { 1 } );
      result_is_ok = (result_inf_is_ok && result_is_ok);
      BOOST_TEST(result_is_ok);

      result_inf_is_ok =
      (
        isinf(riemann_zeta(-inf)) && signbit(riemann_zeta(-inf))
      );
      result_is_ok = (result_inf_is_ok && result_is_ok);
      BOOST_TEST(result_is_ok);
    }

    {
      const decimal_type nan  { ::my_nan <decimal_type>() * decimal_type(dist(gen)) };

      bool result_nan_is_ok { };

      result_nan_is_ok = (isnan(riemann_zeta(nan)));                 result_is_ok = (result_nan_is_ok && result_is_ok); BOOST_TEST(result_is_ok);
      result_nan_is_ok = (isnan(riemann_zeta(-nan)));                result_is_ok = (result_nan_is_ok && result_is_ok); BOOST_TEST(result_is_ok);
      result_nan_is_ok = (isnan(riemann_zeta(decimal_type { 1 })));  result_is_ok = (result_nan_is_ok && result_is_ok); BOOST_TEST(result_is_ok);
    }

    {
      const decimal_type zero { ::my_zero<decimal_type>() * decimal_type(dist(gen)) };

      const decimal_type minus_half { -5, -1 };

      const bool result_zero_is_ok = (riemann_zeta(zero) == minus_half); result_is_ok = (result_zero_is_ok && result_is_ok); BOOST_TEST(result_is_ok);
    }

    {
      const int n_one { static_cast<int>(::my_one<decimal_type>() + decimal_type(dist(gen) / 150.0F)) };

      const decimal_type one { n_one };

      const bool result_one_is_ok = isnan(riemann_zeta(one)); result_is_ok = (result_one_is_ok && result_is_ok); BOOST_TEST(result_is_ok);
    }

    {
      const decimal_type asymp { sqrt((std::numeric_limits<decimal_type>::max)()) * static_cast<decimal_type>(dist(gen)) };

      const bool result_asymp_is_ok = (riemann_zeta(asymp) == ::my_one<decimal_type>()); result_is_ok = (result_asymp_is_ok && result_is_ok); BOOST_TEST(result_is_ok);
    }
  }

  for(auto n = static_cast<unsigned>(UINT8_C(2)); n < static_cast<unsigned>(UINT8_C(6)); ++n)
  {
    using ::boost::decimal::riemann_zeta;

    const decimal_type rzf = riemann_zeta(static_cast<decimal_type>(n));
    const decimal_type rzn = riemann_zeta<decimal_type>(n);

    const bool result_n_or_f_is_ok = (rzf == rzn);

    result_is_ok = (result_n_or_f_is_ok && result_is_ok);

    BOOST_TEST(result_is_ok);
  }

  return result_is_ok;
}

auto test_riemann_zeta_128_lo(const int tol_factor) -> bool
{
  using decimal_type = boost::decimal::decimal128_t;

  using str_ctrl_array_type = std::array<const char*, 3U>;

  const str_ctrl_array_type ctrl_strings =
  {{
      // Table[N[Zeta[1 + n/1000], 36], {n, 5, 7, 1}]
      "200.577579622956683652084654605524346",
      "167.244319052140751595350397994287573",
      "143.434867995431699170218293588670480",
  }};

  std::array<decimal_type, std::tuple_size<str_ctrl_array_type>::value> rz_values   { };
  std::array<decimal_type, std::tuple_size<str_ctrl_array_type>::value> ctrl_values { };

  int nx { 5 };

  bool result_is_ok { true };

  const decimal_type my_tol { std::numeric_limits<decimal_type>::epsilon() * static_cast<decimal_type>(tol_factor) };

  for(auto i = static_cast<std::size_t>(UINT8_C(0)); i < std::tuple_size<str_ctrl_array_type>::value; ++i)
  {
    const decimal_type x_arg =
      decimal_type
      {
          decimal_type { 1 }
        + decimal_type { nx, -3 }
      };

      ++nx;

      rz_values[i] = riemann_zeta(x_arg);

      static_cast<void>
      (
        from_chars(ctrl_strings[i], ctrl_strings[i] + std::strlen(ctrl_strings[i]), ctrl_values[i])
      );

    const auto result_rz_is_ok = is_close_fraction(rz_values[i], ctrl_values[i], my_tol);

    result_is_ok = (result_rz_is_ok && result_is_ok);
  }

  return result_is_ok;
}

auto test_riemann_zeta_128_hi(const int tol_factor) -> bool
{
  using decimal_type = boost::decimal::decimal128_t;

  using str_ctrl_array_type = std::array<const char*, 9U>;

  const str_ctrl_array_type ctrl_strings =
  {{
      // Table[N[Zeta[n + n/10], 36], {n, 1, 9, 1}]
      "10.5844484649508098263864007917355230",
      "1.49054325650689350825344649551165452",
      "1.15194479472077368855082683374115056",
      "1.05928172597983541766404502818685201",
      "1.02520457995468569459240582819540529",
      "1.01116101415427096427312532266653516",
      "1.00504987929596499812178165124883599",
      "1.00231277790982194674469422849347780",
      "1.00106679698357801585766465214764188",
  }};

  std::array<decimal_type, std::tuple_size<str_ctrl_array_type>::value> rz_values   { };
  std::array<decimal_type, std::tuple_size<str_ctrl_array_type>::value> ctrl_values { };

  int nx { 1 };

  bool result_is_ok { true };

  const decimal_type my_tol { std::numeric_limits<decimal_type>::epsilon() * static_cast<decimal_type>(tol_factor) };

  for(auto i = static_cast<std::size_t>(UINT8_C(0)); i < std::tuple_size<str_ctrl_array_type>::value; ++i)
  {
    const decimal_type x_arg =
      decimal_type
      {
          decimal_type { nx }
        + decimal_type { nx, -1 }
      };

      ++nx;

      rz_values[i] = riemann_zeta(x_arg);

      static_cast<void>
      (
        from_chars(ctrl_strings[i], ctrl_strings[i] + std::strlen(ctrl_strings[i]), ctrl_values[i])
      );

    const auto result_rz_is_ok = is_close_fraction(rz_values[i], ctrl_values[i], my_tol);

    result_is_ok = (result_rz_is_ok && result_is_ok);
  }

  return result_is_ok;
}

} // namespace local

int main()
{
  bool result_is_ok { true };

  {
      using decimal_type = ::boost::decimal::decimal32_t;

      const bool result_edge_is_ok = local::test_riemann_zeta_edge<decimal_type, float>();

      result_is_ok = (result_edge_is_ok && result_is_ok);

      BOOST_TEST(result_is_ok);
  }

  {
      using decimal_type = ::boost::decimal::decimal32_t;

      const bool result_rz32_is_ok = local::test_riemann_zeta<decimal_type, float>(128, 1.1L, 5.6L);

      result_is_ok = (result_rz32_is_ok && result_is_ok);

      BOOST_TEST(result_is_ok);
  }

  {
      using decimal_type = ::boost::decimal::decimal32_t;

      const bool result_rz32_is_ok = local::test_riemann_zeta<decimal_type, float>(1024, 1.01L, 1.1L);

      result_is_ok = (result_rz32_is_ok && result_is_ok);

      BOOST_TEST(result_is_ok);
  }

  {
      using decimal_type = ::boost::decimal::decimal_fast32_t;

      const bool result_rz32_is_ok = local::test_riemann_zeta<decimal_type, float>(1024, 1.01L, 1.1L);

      result_is_ok = (result_rz32_is_ok && result_is_ok);

      BOOST_TEST(result_is_ok);
  }

  {
      using decimal_type = ::boost::decimal::decimal32_t;

      const bool result_rz32_is_ok = local::test_riemann_zeta<decimal_type, float>(512, -3.6L, -2.3L);

      result_is_ok = (result_rz32_is_ok && result_is_ok);

      BOOST_TEST(result_is_ok);
  }

  {
      using decimal_type = ::boost::decimal::decimal64_t;

      const bool result_rz64_is_ok = local::test_riemann_zeta<decimal_type, double>(256, 1.1L, 12.3L);

      result_is_ok = (result_rz64_is_ok && result_is_ok);

      BOOST_TEST(result_is_ok);
  }

  {
      using decimal_type = ::boost::decimal::decimal64_t;

      const bool result_rz64_is_ok = local::test_riemann_zeta<decimal_type, double>(1024, 1.01L, 1.1L);

      result_is_ok = (result_rz64_is_ok && result_is_ok);

      BOOST_TEST(result_is_ok);
  }

  {
    using decimal_type = ::boost::decimal::decimal_fast64_t;

    const bool result_rz64_is_ok = local::test_riemann_zeta<decimal_type, double>(256, 1.1L, 12.3L);

    result_is_ok = (result_rz64_is_ok && result_is_ok);

    BOOST_TEST(result_is_ok);
  }

  {
    using decimal_type = ::boost::decimal::decimal_fast64_t;

    const bool result_rz64_is_ok = local::test_riemann_zeta<decimal_type, double>(1024, 1.01L, 1.1L);

    result_is_ok = (result_rz64_is_ok && result_is_ok);

    BOOST_TEST(result_is_ok);
  }

  {
    const auto result_rz_128_lo_is_ok   = local::test_riemann_zeta_128_lo(4096);
    const auto result_rz_128_hi_is_ok   = local::test_riemann_zeta_128_hi(4096);

    BOOST_TEST(result_rz_128_lo_is_ok);
    BOOST_TEST(result_rz_128_hi_is_ok);

    result_is_ok = (result_rz_128_lo_is_ok && result_rz_128_hi_is_ok && result_is_ok);
  }

  result_is_ok = ((boost::report_errors() == 0) && result_is_ok);

  return (result_is_ok ? 0 : -1);
}

template<typename DecimalType> auto my_zero() -> DecimalType& { using decimal_type = DecimalType; static decimal_type val_zero { 0 }; return val_zero; }
template<typename DecimalType> auto my_one () -> DecimalType& { using decimal_type = DecimalType; static decimal_type val_one  { 1 }; return val_one; }
template<typename DecimalType> auto my_nan () -> DecimalType& { using decimal_type = DecimalType; static decimal_type val_nan  { std::numeric_limits<decimal_type>::quiet_NaN() }; return val_nan; }
template<typename DecimalType> auto my_inf () -> DecimalType& { using decimal_type = DecimalType; static decimal_type val_inf  { std::numeric_limits<decimal_type>::infinity() }; return val_inf; }

#ifdef _MSC_VER
# pragma warning(pop)
#endif
