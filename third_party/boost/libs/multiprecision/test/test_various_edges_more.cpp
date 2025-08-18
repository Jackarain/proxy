// Copyright 2023 - 2025 Matt Borland
// Copyright 2023 - 2025 Christopher Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

// Some parts of this test file have been taken from the Boost.Decimal project.

#include <boost/multiprecision/cpp_dec_float.hpp>
#include <boost/multiprecision/cpp_bin_float.hpp>

#include <test_traits.hpp> // Note: include this AFTER the test-backends are defined

#include <chrono>
#include <limits>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wfloat-equal"
#elif defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#endif

#include <boost/core/lightweight_test.hpp>

template<typename FloatType> auto my_zero() noexcept -> FloatType&;
template<typename FloatType> auto my_one () noexcept -> FloatType&;
template<typename FloatType> auto my_inf () noexcept -> FloatType&;
template<typename FloatType> auto my_nan () noexcept -> FloatType&;

namespace local
{
  template<typename IntegralTimePointType,
           typename ClockType = std::chrono::high_resolution_clock>
  auto time_point() noexcept -> IntegralTimePointType
  {
    using local_integral_time_point_type = IntegralTimePointType;
    using local_clock_type               = ClockType;

    typename local_clock_type::time_point tp_zero { };

    const auto my_now = local_clock_type::now();

    const auto duration { my_now - tp_zero };

    const std::uintmax_t
      value
      {
        static_cast<std::uintmax_t>
        (
          std::chrono::duration_cast<std::chrono::nanoseconds>(my_now - tp_zero).count()
        )
      };

    return static_cast<local_integral_time_point_type>(value);
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

    return result_is_ok;
  }

  template<class FloatType>
  bool test_edges()
  {
    using float_type = FloatType;

    std::mt19937_64 gen { time_point<typename std::mt19937_64::result_type>() };

    auto dis =
      std::uniform_real_distribution<float>
      {
        static_cast<float>(1.01F),
        static_cast<float>(1.04F)
      };

    bool result_is_ok { true };

    using std::isinf;
    using std::isnan;
    using std::signbit;

    {
      for(auto index = static_cast<unsigned>(UINT8_C(0)); index < static_cast<unsigned>(UINT8_C(16)); ++index)
      {
        static_cast<void>(index);

        float_type flt_non_finite { };
        long double ldbl_non_finite { };
        bool result_non_finite_is_ok { };

        flt_non_finite = std::numeric_limits<float_type>::quiet_NaN() * dis(gen);
        ldbl_non_finite = static_cast<long double>(flt_non_finite);
        result_non_finite_is_ok = (isnan)(ldbl_non_finite);
        BOOST_TEST(result_non_finite_is_ok);
        result_is_ok = (result_non_finite_is_ok && result_is_ok);

        flt_non_finite = std::numeric_limits<float_type>::infinity() * dis(gen);
        ldbl_non_finite = static_cast<long double>(flt_non_finite);
        result_non_finite_is_ok = (isinf)(ldbl_non_finite);
        BOOST_TEST(result_non_finite_is_ok);
        result_is_ok = (result_non_finite_is_ok && result_is_ok);

        flt_non_finite = std::numeric_limits<float_type>::infinity() * -dis(gen);
        ldbl_non_finite = static_cast<long double>(flt_non_finite);
        result_non_finite_is_ok = ((isinf)(ldbl_non_finite) && signbit(ldbl_non_finite));
        BOOST_TEST(result_non_finite_is_ok);
        result_is_ok = (result_non_finite_is_ok && result_is_ok);

        flt_non_finite =   float_type { (std::numeric_limits<long double>::max)() }
                         * float_type { (std::numeric_limits<long double>::max)() }
                         * dis(gen);
        ldbl_non_finite = static_cast<long double>(flt_non_finite);
        result_non_finite_is_ok = (isinf)(ldbl_non_finite);
        BOOST_TEST(result_non_finite_is_ok);
        result_is_ok = (result_non_finite_is_ok && result_is_ok);

        flt_non_finite =   float_type { (std::numeric_limits<long double>::max)() }
                         * float_type { (std::numeric_limits<long double>::max)() }
                         * -dis(gen);
        ldbl_non_finite = static_cast<long double>(flt_non_finite);
        result_non_finite_is_ok = ((isinf)(ldbl_non_finite) && signbit(ldbl_non_finite));
        BOOST_TEST(result_non_finite_is_ok);
        result_is_ok = (result_non_finite_is_ok && result_is_ok);
      }
    }

    #ifdef BOOST_HAS_INT128
    {
      for(auto index = static_cast<unsigned>(UINT8_C(0)); index < static_cast<unsigned>(UINT8_C(64)); ++index)
      {
        float_type
          flt_n128
          {
              float_type { (std::numeric_limits<signed long long>::max)() }
            * float_type { dis(gen) * 12345.0F }
            * float_type { dis(gen) * dis(gen) }
          };

        const bool is_neg { ((index & 1U) != 0U) };

        if(is_neg)
        {
          flt_n128 = -flt_n128;
        }

        boost::int128_type val_n128 { static_cast<boost::int128_type>(flt_n128) };

        bool result_val_n128_is_ok { };

        const auto
          str128_maker
          {
            [](const float_type& flt)
            {
              std::stringstream strm { };

              strm << std::fixed << flt;

              std::string str_local { strm.str() };

              str_local = str_local.substr(std::size_t { UINT8_C(0) }, str_local.find('.'));

              return str_local;
            }
          };

        const std::string str_ctrl { str128_maker(flt_n128) };
        const std::string str_n128 { str128_maker(static_cast<float_type>(val_n128)) };

        result_val_n128_is_ok =
          (
               (str_n128 == str_ctrl)
            && (
                 (!is_neg) ? (val_n128 > static_cast<boost::int128_type>((std::numeric_limits<signed long long>::max)()))
                           : (val_n128 < static_cast<boost::int128_type>((std::numeric_limits<signed long long>::max)()))
               )
          );

        BOOST_TEST(result_val_n128_is_ok);

        result_is_ok = (result_val_n128_is_ok && result_is_ok);
      }
    }

    {
      for(auto index = static_cast<unsigned>(UINT8_C(0)); index < static_cast<unsigned>(UINT8_C(32)); ++index)
      {
        constexpr boost::int128_type my_max_val_n128 = (((static_cast<boost::int128_type>(1) << (sizeof(boost::int128_type) * CHAR_BIT - 2)) - 1) << 1) + 1;
        constexpr boost::int128_type my_min_val_n128 = static_cast<boost::int128_type>(-my_max_val_n128 - 1);

        const float_type
          flt_n128
          {
              float_type { -my_max_val_n128 }
              - ::my_one<float_type>()
              + float_type { ::my_zero<float_type>() + float_type { dis(gen) } }
              - ::my_one<float_type>()
              - ::my_one<float_type>()
          };

        const boost::int128_type val_n128 { static_cast<boost::int128_type>(flt_n128) };

        const bool result_val_n128_is_ok { (val_n128 == my_min_val_n128) };

        BOOST_TEST(result_val_n128_is_ok);

        result_is_ok = (result_val_n128_is_ok && result_is_ok);
      }
    }

    {
      for(auto index = static_cast<unsigned>(UINT8_C(0)); index < static_cast<unsigned>(UINT8_C(16)); ++index)
      {
        static_cast<void>(index);

        const float_type flt_under_one { float_type { ::my_one<float_type>() / dis(gen) } / dis(gen) };

        const bool
          result_under_one_is_ok
          {
            (static_cast<boost::int128_type>(flt_under_one) == static_cast<boost::int128_type>(INT8_C(0)))
          };

        BOOST_TEST(result_under_one_is_ok);

        result_is_ok = (result_under_one_is_ok && result_is_ok);
      }
    }
    #endif

    {
      const std::initializer_list<std::string>
        funky_strings_list
        {
          std::string("3.14u"),   std::string("3.14l"),   std::string("3.14U"),   std::string("3.14L"),
          std::string("3.14ul"),  std::string("3.14Ul"),  std::string("3.14uL"),  std::string("3.14UL"),
          std::string("3.14ull"), std::string("3.14Ull"), std::string("3.14ULl"), std::string("3.14ULL"), std::string("3.14uLL"),  std::string("3.14ulL")
        };

      const std::vector<std::string> funky_strings(funky_strings_list);

      std::size_t funky_count { };

      for(const auto& str : funky_strings)
      {
        try
        {
          const float_type flt_from_bad_string(str);

          static_cast<void>(flt_from_bad_string);
        }
        catch(const std::runtime_error& excp)
        {
          static_cast<void>(excp.what());

          ++funky_count;
        }
      }

      const bool result_funky_strings_is_ok { (funky_count == funky_strings.size()) };

      BOOST_TEST(result_funky_strings_is_ok);

      result_is_ok = (result_funky_strings_is_ok && result_is_ok);
    }

    {
      using std::ldexp;

      float_type flt_near_max { ldexp((std::numeric_limits<float_type>::max)(), -1) };

      const float_type flt_less_near_max { ldexp((std::numeric_limits<float_type>::max)(), -4) };

      unsigned index { };

      while((index < 1024U) && (!(boost::multiprecision::isinf)(flt_near_max)))
      {
        flt_near_max += (flt_less_near_max * dis(gen));

        ++index;
      }

      const bool result_ovf_is_ok { ((index > 1U) && (index < 1024U)) };

      BOOST_TEST(result_ovf_is_ok);

      result_is_ok = (result_ovf_is_ok && result_is_ok);
    }

    {
      using std::ldexp;

      float_type flt_near_lowest { -ldexp((std::numeric_limits<float_type>::max)(), -1) };

      const float_type flt_less_near_max {  ldexp((std::numeric_limits<float_type>::max)(), -4) };

      unsigned index { };

      while((index < 1024U) && (!(boost::multiprecision::isinf)(flt_near_lowest)))
      {
        flt_near_lowest -= (flt_less_near_max * dis(gen));

        ++index;
      }

      const bool result_ovf_is_ok { ((index > 1U) && (index < 1024U)) && signbit(flt_near_lowest) };

      BOOST_TEST(result_ovf_is_ok);

      result_is_ok = (result_ovf_is_ok && result_is_ok);
    }

    for(int n_loop = static_cast<int>(INT8_C(-16)); n_loop < static_cast<int>(INT8_C(-8)); ++n_loop)
    {
      using std::ldexp;
      using std::fabs;

      float_type flt_near_max { ldexp((std::numeric_limits<float_type>::max)(), n_loop) * dis(gen) };

      unsigned index { };

      while((index < 1024U) && (!(boost::multiprecision::isinf)(flt_near_max)))
      {
        flt_near_max *= static_cast<unsigned long long>(INT32_C(2));

        ++index;
      }

      const bool result_ovf_is_ok { ((index > 1U) && (index < 1024U)) };

      BOOST_TEST(result_ovf_is_ok);

      result_is_ok = (result_ovf_is_ok && result_is_ok);
    }

    {
      for(auto index = static_cast<unsigned>(UINT8_C(0)); index < static_cast<unsigned>(UINT8_C(16)); ++index)
      {
        static_cast<void>(index);

        const float_type flt_nan_denominator { float_type { std::numeric_limits<float>::quiet_NaN() } * dis(gen) };

        float_type flt_zero_numerator { ::my_zero<float_type>() * dis(gen) };

        const bool
          result_div_zero_by_nan_is_ok
          {
               (boost::multiprecision::isnan)(flt_nan_denominator)
            && (boost::multiprecision::isnan)(flt_zero_numerator /= flt_nan_denominator)
          };

        BOOST_TEST(result_div_zero_by_nan_is_ok);

        result_is_ok = (result_div_zero_by_nan_is_ok && result_is_ok);
      }
    }

    {
      for(auto index = static_cast<unsigned>(UINT8_C(0)); index < static_cast<unsigned>(UINT8_C(16)); ++index)
      {
        static_cast<void>(index);

        float_type flt_zero_numerator { ::my_zero<float_type>() * dis(gen) };
        float_type flt_finite_numerator { ::my_one<float_type>() * dis(gen) };
        float_type flt_nan_numerator { ::my_nan<float_type>() * dis(gen) };

        const float_type flt_infinite_result_neg { -flt_finite_numerator /= static_cast<signed long long>(INT8_C(0)) };

        const bool result_edge_00 { (boost::multiprecision::isnan)(flt_zero_numerator /= static_cast<signed long long>(INT8_C(0))) };
        const bool result_edge_01 { (boost::multiprecision::isinf)(flt_finite_numerator /= static_cast<signed long long>(INT8_C(0))) };
        const bool result_edge_02 { (boost::multiprecision::isinf)(flt_infinite_result_neg) && (boost::multiprecision::signbit)(flt_infinite_result_neg) };
        const bool result_edge_03 { (boost::multiprecision::isnan)(flt_nan_numerator /= static_cast<signed long long>(index + 2U)) };

        const bool
          result_divs_are_ok
          {
            (result_edge_00 && result_edge_01 && result_edge_02 && result_edge_03)
          };

        BOOST_TEST(result_divs_are_ok);

        result_is_ok = (result_divs_are_ok && result_is_ok);
      }
    }

    return result_is_ok;
  }

} // namespace local

auto main() -> int
{
  {
    using float_backend_type = boost::multiprecision::cpp_bin_float<50>;

    using float_type = boost::multiprecision::number<float_backend_type, boost::multiprecision::et_off>;

    std::cout << "Testing type: " << typeid(float_type).name() << std::endl;

    static_cast<void>(local::test_edges<float_type>());
  }

  {
    using float_backend_type = boost::multiprecision::cpp_dec_float<50>;

    using float_type = boost::multiprecision::number<float_backend_type, boost::multiprecision::et_off>;

    std::cout << "Testing type: " << typeid(float_type).name() << std::endl;

    static_cast<void>(local::test_edges<float_type>());
  }

  return boost::report_errors();
}

template<typename FloatType> auto my_zero() noexcept -> FloatType& { using float_type = FloatType; static float_type val_zero { 0 }; return val_zero; }
template<typename FloatType> auto my_one () noexcept -> FloatType& { using float_type = FloatType; static float_type val_one  { 1 }; return val_one; }
template<typename FloatType> auto my_inf () noexcept -> FloatType& { using float_type = FloatType; static float_type val_inf  { std::numeric_limits<float_type>::infinity() }; return val_inf; }
template<typename FloatType> auto my_nan () noexcept -> FloatType& { using float_type = FloatType; static float_type val_nan  { std::numeric_limits<float_type>::quiet_NaN() }; return val_nan; }

#if defined(__clang__)
#  pragma clang diagnostic pop
#elif defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif
