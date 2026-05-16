// Copyright 2024 - 2026 Matt Borland
// Copyright 2024 - 2026 Christopher Kormanyos
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

    const NumericType delta
    {
      (b == static_cast<NumericType>(0)) ? fabs(a - b) : fabs(1 - (a / b))
    };

    const bool result_is_ok { (delta < tol) };

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
  auto test_cbrt(const std::int32_t tol_factor, const long double range_lo, const long double range_hi) -> bool
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

    auto dis_sign =
      std::uniform_int_distribution<int>
      {
        0,
        1
      };

    auto result_is_ok = true;

    auto trials = static_cast<std::uint32_t>(UINT8_C(0));

    #if !defined(BOOST_DECIMAL_REDUCE_TEST_DEPTH)
    constexpr std::uint32_t count { ((std::numeric_limits<DecimalType>::digits10 < 10) ? UINT16_C(3200) : UINT16_C(1600)) };
    #else
    constexpr std::uint32_t count { ((std::numeric_limits<DecimalType>::digits10 < 10) ? UINT16_C(320)  : UINT16_C(160)) };
    #endif

    for( ; trials < count; ++trials)
    {
      const bool is_neg = (dis_sign(gen) == 1);

      const float_type   x_flt = (is_neg ? -dis(gen) : dis(gen));
      const decimal_type x_dec = static_cast<decimal_type>(x_flt);

      using std::cbrt;

      const auto val_flt = cbrt(x_flt);
      const auto val_dec = cbrt(x_dec);

      const auto result_val_is_ok = is_close_fraction(val_flt, static_cast<float_type>(val_dec), std::numeric_limits<float_type>::epsilon() * static_cast<float_type>(tol_factor));

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

  // https://github.com/boostorg/decimal/issues/440
  template <typename T>
  void test_issue440()
  {
      BOOST_TEST(cbrt(T{8})   == T{2});
      BOOST_TEST(cbrt(T{27})  == T{3});
      BOOST_TEST(cbrt(T{64})  == T{4});
      BOOST_TEST(cbrt(T{125}) == T{5});
      BOOST_TEST(cbrt(T{216}) == T{6});
  }

  template<typename DecimalType, typename FloatType>
  auto test_cbrt_edge() -> bool
  {
    using decimal_type = DecimalType;
    using float_type   = FloatType;

    std::mt19937_64 gen;

    std::uniform_real_distribution<float_type> dist(static_cast<float_type>(1.01L), static_cast<float_type>(1.04L));

    auto result_is_ok = true;

    {
      int np = -33;

      for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(67)); ++i)
      {
        static_cast<void>(i);

        const decimal_type arg_p10 { 1, np };

        decimal_type val_p10 = cbrt(arg_p10);

        bool result_val_p10_is_ok { };

        const int np_mod3 = np % 3;
        const int np_div3 = np / 3;

        if(np_mod3 == 0)
        {
          result_val_p10_is_ok = (val_p10 == decimal_type { 1, np_div3 });
        }
        else
        {
          decimal_type val_p10_ctrl { 1, np_div3 };

          switch (np_mod3)
          {
            case 2:
                val_p10_ctrl *= boost::decimal::numbers::cbrt10_v<decimal_type>;
                // fallthrough

            case 1:
                val_p10_ctrl *= boost::decimal::numbers::cbrt10_v<decimal_type>;
                break;

            case -2:
                val_p10_ctrl /= boost::decimal::numbers::cbrt10_v<decimal_type>;
                // fallthrough

            case -1:
                val_p10_ctrl /= boost::decimal::numbers::cbrt10_v<decimal_type>;
                break;
          }

          result_val_p10_is_ok = (val_p10 == val_p10_ctrl);
        }

        ++np;

        BOOST_TEST(result_val_p10_is_ok);

        result_is_ok = (result_val_p10_is_ok && result_is_ok);
      }
    }

    {
      for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(10)); ++i)
      {
        static_cast<void>(i);

        const decimal_type inf     { std::numeric_limits<decimal_type>::infinity() * static_cast<int>(dist(gen)) };
        const decimal_type nan     { std::numeric_limits<decimal_type>::quiet_NaN() * static_cast<int>(dist(gen)) };
        const decimal_type zero    { decimal_type { 0 } * static_cast<int>(dist(gen)) };
        const decimal_type neg_arg { -static_cast<int>(dist(gen)) };

        BOOST_TEST(isinf(cbrt(inf)));
        BOOST_TEST(isinf(cbrt(-inf)));
        BOOST_TEST(isnan(cbrt(nan)));
        BOOST_TEST(isnan(cbrt(-nan)));

        BOOST_TEST_EQ(cbrt(zero), zero);
        BOOST_TEST_EQ(cbrt(-zero), -zero);
        BOOST_TEST_EQ(cbrt(neg_arg), -cbrt(-neg_arg));
      }
    }

    return result_is_ok;
  }

  auto test_cbrt_128(const int tol_factor) -> bool
  {
    using decimal_type = boost::decimal::decimal128_t;

    using str_ctrl_array_type = std::array<const char*, 41U>;

    const str_ctrl_array_type ctrl_strings =
    {{
       // Table[N[(123456 (10^n))^(1/3), 36], {n, -20, 20, 1}]
       "0.0000107276369432283170454869317373527648",
       "0.0000231119931725586838245479638915411868",
       "0.0000497932798467404808519645606333557921",
       "0.000107276369432283170454869317373527648",
       "0.000231119931725586838245479638915411868",
       "0.000497932798467404808519645606333557921",
       "0.00107276369432283170454869317373527648",
       "0.00231119931725586838245479638915411868",
       "0.00497932798467404808519645606333557921",
       "0.0107276369432283170454869317373527648",
       "0.0231119931725586838245479638915411868",
       "0.0497932798467404808519645606333557921",
       "0.107276369432283170454869317373527648",
       "0.231119931725586838245479638915411868",
       "0.497932798467404808519645606333557921",
       "1.07276369432283170454869317373527648",
       "2.31119931725586838245479638915411868",
       "4.97932798467404808519645606333557921",
       "10.7276369432283170454869317373527648",
       "23.1119931725586838245479638915411868",
       "49.7932798467404808519645606333557921",
       "107.276369432283170454869317373527648",
       "231.119931725586838245479638915411868",
       "497.932798467404808519645606333557921",
       "1072.76369432283170454869317373527648",
       "2311.19931725586838245479638915411868",
       "4979.32798467404808519645606333557921",
       "10727.6369432283170454869317373527648",
       "23111.9931725586838245479638915411868",
       "49793.2798467404808519645606333557921",
       "107276.369432283170454869317373527648",
       "231119.931725586838245479638915411868",
       "497932.798467404808519645606333557921",
       "1.07276369432283170454869317373527648E6",
       "2.31119931725586838245479638915411868E6",
       "4.97932798467404808519645606333557921E6",
       "1.07276369432283170454869317373527648E7",
       "2.31119931725586838245479638915411868E7",
       "4.97932798467404808519645606333557921E7",
       "1.07276369432283170454869317373527648E8",
       "2.31119931725586838245479638915411868E8",
    }};

    std::array<decimal_type, std::tuple_size<str_ctrl_array_type>::value> cbrt_values { };
    std::array<decimal_type, std::tuple_size<str_ctrl_array_type>::value> ctrl_values { };

    int nx { -20 };

    bool result_is_ok { true };

    const decimal_type my_tol { std::numeric_limits<decimal_type>::epsilon() * static_cast<decimal_type>(tol_factor) };

    for(auto i = static_cast<std::size_t>(UINT8_C(0)); i < std::tuple_size<str_ctrl_array_type>::value; ++i)
    {
      const decimal_type x_arg = decimal_type { 123456L, nx };

      ++nx;

      cbrt_values[i] = cbrt(x_arg);

      static_cast<void>
      (
        from_chars(ctrl_strings[i], ctrl_strings[i] + std::strlen(ctrl_strings[i]), ctrl_values[i])
      );

      const auto result_cbrt_is_ok = is_close_fraction(cbrt_values[i], ctrl_values[i], my_tol);

      result_is_ok = (result_cbrt_is_ok && result_is_ok);
    }

    return result_is_ok;
  }

} // namespace local

int main()
{
  bool result_is_ok { true };

  {
    using decimal_type = boost::decimal::decimal32_t;
    using float_type   = float;

    const auto result_small_is_ok  = local::test_cbrt<decimal_type, float_type>(16, 1.0E-26L, 1.0E-01L);
    const auto result_medium_is_ok = local::test_cbrt<decimal_type, float_type>(16, 0.9E-02L, 1.1E+01L);
    const auto result_large_is_ok  = local::test_cbrt<decimal_type, float_type>(16, 1.0E+01L, 1.0E+26L);

    BOOST_TEST(result_small_is_ok);
    BOOST_TEST(result_medium_is_ok);
    BOOST_TEST(result_large_is_ok);

    const auto result_edge_is_ok = local::test_cbrt_edge<decimal_type, float_type>();

    const auto result_ranges_is_ok = (result_small_is_ok && result_medium_is_ok && result_large_is_ok);

    result_is_ok = (result_ranges_is_ok && result_is_ok);

    BOOST_TEST(result_edge_is_ok);

    result_is_ok = (result_edge_is_ok && result_is_ok);
  }

  {
    using decimal_type = boost::decimal::decimal64_t;
    using float_type   = double;

    const auto result_small_is_ok  = local::test_cbrt<decimal_type, float_type>(16, 1.0E-26L, 1.0E-01L);
    const auto result_medium_is_ok = local::test_cbrt<decimal_type, float_type>(16, 0.9E-01L, 1.1E+01L);
    const auto result_large_is_ok  = local::test_cbrt<decimal_type, float_type>(16, 1.0E+01L, 1.0E+26L);

    BOOST_TEST(result_small_is_ok);
    BOOST_TEST(result_medium_is_ok);
    BOOST_TEST(result_large_is_ok);

    const auto result_edge_is_ok = local::test_cbrt_edge<decimal_type, float_type>();

    const auto result_ranges_is_ok = (result_small_is_ok && result_medium_is_ok && result_large_is_ok);

    result_is_ok = (result_ranges_is_ok && result_is_ok);

    BOOST_TEST(result_edge_is_ok);

    result_is_ok = (result_edge_is_ok && result_is_ok);
  }

  {
    using decimal_type = boost::decimal::decimal_fast64_t;
    using float_type   = double;

    const auto result_small_is_ok  = local::test_cbrt<decimal_type, float_type>(INT32_C(16), 1.0E-76L, 1.0E-01L);
    const auto result_medium_is_ok = local::test_cbrt<decimal_type, float_type>(INT32_C(16), 0.9E-01L, 1.1E+01L);
    const auto result_large_is_ok  = local::test_cbrt<decimal_type, float_type>(INT32_C(16), 1.0E+01L, 1.0E+76L);

    BOOST_TEST(result_small_is_ok);
    BOOST_TEST(result_medium_is_ok);
    BOOST_TEST(result_large_is_ok);

    const auto result_edge_is_ok = local::test_cbrt_edge<decimal_type, float_type>();

    const auto result_ranges_is_ok = (result_small_is_ok && result_medium_is_ok && result_large_is_ok);

    result_is_ok = (result_ranges_is_ok && result_is_ok);

    BOOST_TEST(result_edge_is_ok);

    result_is_ok = (result_edge_is_ok && result_is_ok);
  }

  {
    const auto result_cbrt128_is_ok = local::test_cbrt_128(16);

    BOOST_TEST(result_cbrt128_is_ok);

    result_is_ok = (result_cbrt128_is_ok && result_is_ok);
  }

  BOOST_TEST(result_is_ok);

  return boost::report_errors();
}
