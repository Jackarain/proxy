// Copyright 2026 Christopher Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#if !defined(BOOST_MP_STANDALONE)
#define BOOST_MP_STANDALONE
#endif

#if !defined(BOOST_MATH_STANDALONE)
#define BOOST_MATH_STANDALONE
#endif

#if !defined(BOOST_MATH_NO_RTTI)
#define BOOST_MATH_NO_RTTI
#endif

#if !defined(BOOST_MATH_DISABLE_THREADS)
#define BOOST_MATH_DISABLE_THREADS
#endif

#if !defined(BOOST_MATH_NO_EXCEPTIONS)
#define BOOST_MATH_NO_EXCEPTIONS
#endif

#if !defined(BOOST_MP_NO_EXCEPTIONS)
#define BOOST_MP_NO_EXCEPTIONS
#endif

#if !defined(BOOST_NO_EXCEPTIONS)
#define BOOST_NO_EXCEPTIONS
#endif

#include "testing_config.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <type_traits>

#include <boost/decimal.hpp>

#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wfloat-equal"
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wold-style-cast"
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wundef"
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wunused-parameter"
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wdeprecated-declarations"
#elif defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wold-style-cast"
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wrestrict"
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wuseless-cast"
#endif

#include <boost/math/policies/policy.hpp>
#include <boost/math/special_functions/cbrt.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>

#include <boost/core/lightweight_test.hpp>

namespace boost { namespace math { namespace policies {

// Specialization of the precision structure.
template<typename ThisPolicy>
struct precision<boost::decimal::decimal128_t, ThisPolicy>
{
private:
  using local_decimal_type = boost::decimal::decimal128_t;

  static constexpr auto local_digits2_value() noexcept -> int
  {
    return static_cast<int>
    (
      static_cast<std::intmax_t>
      (
        std::numeric_limits<local_decimal_type>::digits10 * INTMAX_C(1000)
      ) / INTMAX_C(301)
    );
  }

public:
  using local_digits_2 = digits2<local_digits2_value()>;

  using precision_type = typename ThisPolicy::precision_type;

  using type =
    typename std::conditional<((local_digits_2::value <= precision_type::value) || (precision_type::value <= 0)),
                                local_digits_2,
                                precision_type>::type;
};

} } } // namespace boost::math::policies


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

  template<typename ExtendedFloatType>
  auto get_random_float_string(std::string* p_str_flt = nullptr, const bool do_seed_rnd_gens = false) -> ExtendedFloatType; // NOLINT(google-runtime-references)

  template<typename ExtendedFloatType>
  auto get_random_float_string(std::string* p_str_flt, const bool do_seed_rnd_gens) -> ExtendedFloatType // NOLINT(google-runtime-references)
  {
    using local_eng_exp_type = std::minstd_rand;
    using local_eng_man_type = std::mt19937;

    static local_eng_exp_type eng_exp { }; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
    static local_eng_man_type eng_man { }; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

    static std::uniform_int_distribution<signed>   dst_exp { -4000, +4000 };  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
    static std::uniform_int_distribution<unsigned> dst_man { 0x30U, 0x39U };  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

    if(do_seed_rnd_gens)
    {
      eng_exp.seed(time_point<typename local_eng_exp_type::result_type>());
      eng_man.seed(time_point<typename local_eng_man_type::result_type>());
    }

    std::string str { };

    using local_extended_float_type = ExtendedFloatType;

    for(int index = 0; index < std::numeric_limits<local_extended_float_type>::digits10; ++index)
    {
      static_cast<void>(index);

      if(str.length() == std::string::size_type { UINT8_C(0) })
      {
        char chr_nonzero { };

        do
        {
          chr_nonzero = static_cast<char>(dst_man(eng_man));
        }
        while(chr_nonzero == char { INT8_C(0x30) });

        str.push_back(chr_nonzero);
      }
      else if(str.length() == std::string::size_type { UINT8_C(1) })
      {
        str.push_back('.');
      }
      else
      {
        str.push_back(static_cast<char>(dst_man(eng_man)));
      }
    }

    str.push_back('E');

    const int n_exp { dst_exp(eng_exp) };

    std::stringstream strm { };

    strm << n_exp;

    str += strm.str();

    if(p_str_flt != nullptr)
    {
      *p_str_flt = str.c_str();
    }

    return local_extended_float_type { str.c_str() };
  }
} // namespace local

auto main() -> int
{
  using deci_type = boost::decimal::decimal128_t;
  using ctrl_type = boost::multiprecision::number<boost::multiprecision::cpp_dec_float<static_cast<unsigned int>(std::numeric_limits<deci_type>::digits10)>, boost::multiprecision::et_off>;

  bool result_cbrt_ctrl_is_ok { true };

  int index { };

  #if !defined(BOOST_DECIMAL_REDUCE_TEST_DEPTH)
  constexpr int max_trials { 2000 };
  #else
  constexpr int max_trials {  200 };
  #endif

  for(index = 0; index < max_trials; ++index)
  {
    const bool do_seed_rnd_gens { (index == 0) || (((index + 1 ) % 128) == 0) };

    std::string str_flt { };

    const deci_type
      deci
      {
        local::get_random_float_string<deci_type>(&str_flt, do_seed_rnd_gens)
      };

    const ctrl_type ctrl { boost::math::cbrt(ctrl_type { str_flt.c_str() }) };

    const ctrl_type
      test
      {
        [&deci]
        {
          const deci_type local_val { boost::math::cbrt(deci) };

          std::stringstream strm { };

          strm << std::setprecision(std::numeric_limits<deci_type>::digits10)
               << local_val;

          return ctrl_type { strm.str().c_str() };
        }()
      };

    const ctrl_type
      tol_one_eps_deci
      {
        []()
        {
          std::stringstream strm { };

          strm << std::setprecision(std::numeric_limits<deci_type>::digits10)
               << std::numeric_limits<deci_type>::epsilon();

          return ctrl_type { strm.str() };
        }()
      };

    result_cbrt_ctrl_is_ok = local::is_close_fraction(ctrl, test, tol_one_eps_deci * 1024);

    BOOST_TEST(result_cbrt_ctrl_is_ok);

    if(!result_cbrt_ctrl_is_ok)
    {
      // LCOV_EXCL_START
      std::stringstream strm { };

      strm << "str_flt: "
            << std::setprecision(std::numeric_limits<deci_type>::digits10)
            << str_flt
            << ", test: "
            << std::setprecision(std::numeric_limits<deci_type>::digits10)
            << test
            << ", ctrl: "
            << std::setprecision(std::numeric_limits<deci_type>::digits10)
            << ctrl
            ;

      std::cout << strm.str() << std::endl;

      break;
      // LCOV_EXCL_STOP
    }
  }

  const bool result_total_is_ok { (result_cbrt_ctrl_is_ok && (index == max_trials)) };

  BOOST_TEST(result_total_is_ok);

  return boost::report_errors();
}

#if defined(__clang__)
#  pragma clang diagnostic pop
#  pragma clang diagnostic pop
#  pragma clang diagnostic pop
#  pragma clang diagnostic pop
#elif defined(__GNUC__)
#  pragma GCC diagnostic pop
#  pragma GCC diagnostic pop
#  pragma GCC diagnostic pop
#  pragma GCC diagnostic pop
#  pragma GCC diagnostic pop
#endif
