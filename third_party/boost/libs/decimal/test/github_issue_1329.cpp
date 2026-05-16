// Copyright 2026 Christopher Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wfloat-equal"
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wold-style-cast"
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wundef"
#if (__clang_major__ > 12)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wunused-but-set-variable"
#endif
#elif defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wold-style-cast"
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wuseless-cast"
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wrestrict"
#endif

#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable : 4996)
#elif (defined(__clang__) && (__clang_major__ >= 17))
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

#if !defined(BOOST_MP_STANDALONE)
#define BOOST_MP_STANDALONE
#endif

#if !defined(BOOST_MP_NO_EXCEPTIONS)
#define BOOST_MP_NO_EXCEPTIONS
#endif

#include "testing_config.hpp"

#include <boost/decimal/decimal128_t.hpp>
#include <boost/decimal/cmath.hpp>
#include <boost/decimal/iostream.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>

#include <boost/core/lightweight_test.hpp>

#include <chrono>
#include <random>
#include <iomanip>
#include <sstream>

//#define BOOST_DECIMAL_ISSUE_1329_SHOWS_STATS

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

    return result_is_ok;
  }

  auto test_frexp_nohang128() -> void
  {
    std::random_device rd;
    std::mt19937_64 gen(rd());

    gen.seed(time_point<typename std::mt19937_64::result_type>());

    auto dis_denom =
      std::uniform_int_distribution<std::uint32_t>
      {
        UINT32_C(104744),
        UINT32_C(999999)
      };

    auto dis_exp10 =
      std::uniform_int_distribution<std::int32_t>
      {
        INT32_C(-4100),
        INT32_C(+4100)
      };

    bool result_is_ok { true };

    #if defined(BOOST_DECIMAL_ISSUE_1329_SHOWS_STATS)
    std::uint32_t count { };
    double total_time_ns { };
    #endif

    #if !defined(BOOST_DECIMAL_REDUCE_TEST_DEPTH)
    for(std::uint32_t index { UINT8_C(0) }; index < UINT32_C(1024); ++index)
    #else
    for(std::uint32_t index { UINT8_C(0) }; index < UINT32_C(32); ++index)
    #endif
    {
      static_cast<void>(index);

      using mp_type = boost::multiprecision::number<boost::multiprecision::cpp_dec_float<34>, boost::multiprecision::et_off>;

      const auto exp10 { dis_exp10(gen) };
      const auto denom { dis_denom(gen) };

      boost::decimal::decimal128_t arg_x { UINT32_C(104743), exp10 }; arg_x /= denom;
      mp_type arg_mp { UINT32_C(104743) * pow(mp_type(10), exp10) }; arg_mp /= denom;

      const auto start { std::chrono::high_resolution_clock::now() };
      int nexp2 { };
      const boost::decimal::decimal128_t trial { frexp(arg_x, &nexp2) };
      const auto stop { std::chrono::high_resolution_clock::now() };

      const double
        duration
        {
          static_cast<double>
          (
            std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count()
          )
        };

      int n_mp { };
      const mp_type ctrl_mp { frexp(arg_mp, &n_mp) };

      std::string str_ctrl { };

      boost::decimal::decimal128_t ctrl_mp_as_dec { };

      {
        std::stringstream strm { };

        strm << std::setprecision(std::numeric_limits<boost::decimal::decimal128_t>::digits10) << ctrl_mp;

        ctrl_mp_as_dec = boost::decimal::decimal128_t(strm.str());
      }

      result_is_ok = { (nexp2 == n_mp) && is_close_fraction(trial, ctrl_mp_as_dec, std::numeric_limits<boost::decimal::decimal128_t>::epsilon() * 64) };

      BOOST_TEST(result_is_ok);

      #if defined(BOOST_DECIMAL_ISSUE_1329_SHOWS_STATS)
      total_time_ns += duration;

      ++count;

      {
        const double avg_us { static_cast<double>(total_time_ns / count) / 1000.0 };

        std::stringstream strm { };

        strm << std::boolalpha
             << "result_is_ok: "
             << result_is_ok
             << ", count: "
             << count
             << ", avg_ns: "
             << std::fixed
             << std::setprecision(1)
             << avg_us;

        std::cout << strm.str() << std::endl;
      }
      #else
      static_cast<void>(duration);
      #endif

      if(!result_is_ok)
      {
        break;
      }
    }
  }
} // namespace local

auto main() -> int
{
  local::test_frexp_nohang128();

  return boost::report_errors();
}

#ifdef _MSC_VER
#pragma warning(pop)
#elif (defined(__clang__) && (__clang_major__ >= 17))
#  pragma clang diagnostic pop
#endif

#if defined(__clang__)
#  pragma clang diagnostic pop
#  pragma clang diagnostic pop
#  pragma clang diagnostic pop
#if (__clang_major__ > 12)
#  pragma clang diagnostic pop
#endif
#elif defined(__GNUC__)
#  pragma GCC diagnostic pop
#  pragma GCC diagnostic pop
#  pragma GCC diagnostic pop
#  pragma GCC diagnostic pop
#endif
