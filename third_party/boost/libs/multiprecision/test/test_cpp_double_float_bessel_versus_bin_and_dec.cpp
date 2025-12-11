///////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2024 - 2025.
//  Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt

// This test file executes a specific function-by-function
// comparison of cpp_double_double with other backends having
// similar digit counts.

#include <boost/core/lightweight_test.hpp>
#include <boost/math/special_functions/bessel.hpp>
#include <boost/multiprecision/cpp_double_fp.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#if defined(BOOST_HAS_FLOAT128)
#include <boost/multiprecision/float128.hpp>
#endif

#include <cstdint>
#include <ctime>
#include <limits>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <vector>

#if (defined(BOOST_MSVC) && !defined(BOOST_GCC))
#define STOPWATCH_NODISCARD
#else
#if (defined(BOOST_CXX_VERSION) && (BOOST_CXX_VERSION >= 201703L))
#define STOPWATCH_NODISCARD  [[nodiscard]] // NOLINT(cppcoreguidelines-macro-usage)
#else
#define STOPWATCH_NODISCARD
#endif
#endif

namespace concurrency {

// See also: https://godbolt.org/z/37a4n9f4Y

struct stopwatch
{
public:
  using time_point_type = std::uintmax_t;

  auto reset() -> void
  {
    m_start = now();
  }

  template<typename RepresentationRequestedTimeType>
  static auto elapsed_time(const stopwatch& my_stopwatch) -> RepresentationRequestedTimeType
  {
    using local_time_type = RepresentationRequestedTimeType;

    return
      local_time_type
      {
          static_cast<local_time_type>(my_stopwatch.elapsed())
        / local_time_type { UINTMAX_C(1000000000) }
      };
  }

  STOPWATCH_NODISCARD static auto now() -> time_point_type
  {
    #if defined(__CYGWIN__)

    return static_cast<time_point_type>(std::clock());

    #else

    timespec ts { };

    const int ntsp { timespec_get(&ts, TIME_UTC) };

    static_cast<void>(ntsp);

    return
      static_cast<time_point_type>
      (
          static_cast<time_point_type>(static_cast<time_point_type>(ts.tv_sec) * UINTMAX_C(1000000000))
        + static_cast<time_point_type>(ts.tv_nsec)
      );

    #endif
  }

private:
  time_point_type m_start { now() }; // NOLINT(readability-identifier-naming)

  STOPWATCH_NODISCARD auto elapsed() const -> time_point_type
  {
    const time_point_type stop { now() };

    #if defined(__CYGWIN__)

    const time_point_type
      elapsed_ns
      {
        static_cast<time_point_type>
        (
            static_cast<time_point_type>(static_cast<time_point_type>(stop - m_start) * UINTMAX_C(1000000000))
          / static_cast<time_point_type>(CLOCKS_PER_SEC)
        )
      };

    #else

    const time_point_type elapsed_ns { static_cast<time_point_type>(stop - m_start) };

    #endif

    return elapsed_ns;
  }
};

} // namespace concurrency

namespace util {

template<typename UnsignedIntegralType>
auto util_pseudorandom_time_point_seed() -> UnsignedIntegralType
{
  using stopwatch_type = concurrency::stopwatch;

  return static_cast<UnsignedIntegralType>(stopwatch_type::now());
}

} // namespace util

std::uniform_int_distribution<std::uint32_t> dist_sgn(UINT32_C(   0), UINT32_C(    1)); // NOLINT(cert-err58-cpp,cppcoreguidelines-avoid-non-const-global-variables)
std::uniform_int_distribution<std::uint32_t> dist_dig(UINT32_C(0x31), UINT32_C( 0x39)); // NOLINT(cert-err58-cpp,cppcoreguidelines-avoid-non-const-global-variables)

using eng_sgn_type = std::minstd_rand0;
using eng_dig_type = std::mt19937_64;

eng_sgn_type eng_sgn; // NOLINT(cert-msc32-c,cert-msc51-cpp,cert-err58-cpp,cppcoreguidelines-avoid-non-const-global-variables)
eng_dig_type eng_dig; // NOLINT(cert-msc32-c,cert-msc51-cpp,cert-err58-cpp,cppcoreguidelines-avoid-non-const-global-variables)

template<typename FloatingPointType>
auto generate_wide_decimal_value(int digits10_to_get = std::numeric_limits<FloatingPointType>::digits10 - 2) -> std::string
{
  using local_floating_point_type = FloatingPointType;

  static_assert(std::numeric_limits<local_floating_point_type>::digits10 > static_cast<int>(INT8_C(9)),
                "Error: Floating-point type destination does not have enough digits10");

  std::string str_x(static_cast<std::size_t>(digits10_to_get + 1), '0');

  // Get the leading digit before the decimal point.
  str_x.at(std::string::size_type { UINT8_C(0) }) = static_cast<char>(dist_dig(eng_dig));

  // Insert a decimal point.
  str_x.at(std::string::size_type { UINT8_C(1) }) = '.';

  std::generate(str_x.begin() + std::string::size_type { UINT8_C(2) },
                str_x.end(),
                []() // NOLINT(modernize-use-trailing-return-type,-warnings-as-errors)
                {
                  return static_cast<char>(dist_dig(eng_dig));
                });

  return str_x;
} // LCOV_EXCL_LINE

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

auto do_trials(const std::size_t trial_count) -> void
{
  static std::size_t heat_count { };
  static std::size_t total_count { };

  std::cout << "\nheat_count: " << ++heat_count << std::endl;

  using dbl_float_type = boost::multiprecision::cpp_double_double;

  constexpr int local_digits10 = ((std::numeric_limits<dbl_float_type>::digits10 < 32) ? 32 : std::numeric_limits<dbl_float_type>::digits10);

  using dec_float_type = boost::multiprecision::number<boost::multiprecision::cpp_dec_float<local_digits10>, boost::multiprecision::et_off>;
  using bin_float_type = boost::multiprecision::number<boost::multiprecision::cpp_bin_float<local_digits10>, boost::multiprecision::et_off>;
  #if defined(BOOST_HAS_FLOAT128)
  using flt_float_type = boost::multiprecision::float128;
  #endif

  const std::size_t trials { trial_count };

  std::vector<dbl_float_type> dbl_float_a_vec (trials);
  std::vector<dbl_float_type> dbl_float_b_vec (dbl_float_a_vec.size());

  std::vector<dec_float_type> dec_float_a_vec (dbl_float_a_vec.size());
  std::vector<dec_float_type> dec_float_b_vec (dbl_float_a_vec.size());
  std::vector<bin_float_type> bin_float_a_vec (dbl_float_a_vec.size());
  std::vector<bin_float_type> bin_float_b_vec (dbl_float_a_vec.size());
  #if defined(BOOST_HAS_FLOAT128)
  std::vector<flt_float_type> flt_float_a_vec (dbl_float_a_vec.size());
  std::vector<flt_float_type> flt_float_b_vec (dbl_float_a_vec.size());
  #endif

  for(std::size_t index { UINT8_C(0) }; index < dbl_float_a_vec.size(); ++index)
  {
    if(std::size_t { total_count % unsigned { UINT32_C(0x1000) } } == std::size_t { UINT8_C(0) })
    {
      eng_sgn.seed(util::util_pseudorandom_time_point_seed<typename eng_sgn_type::result_type>());
      eng_dig.seed(util::util_pseudorandom_time_point_seed<typename eng_dig_type::result_type>());
    }

    ++total_count;

    auto
      gen
      {
        [](bool is_positive, const dbl_float_type& gate = 0 )
        {
          dbl_float_type val { };

          if(gate != 0)
          {
            while((val = dbl_float_type(generate_wide_decimal_value<dbl_float_type>())) < gate) { ; }
          }
          else
          {
            val = dbl_float_type(generate_wide_decimal_value<dbl_float_type>()); // LCOV_EXCL_LINE
          }

          if(is_positive)
          {
            return val;
          }
          // LCOV_EXCL_START
          else
          {
            const bool next_positive { (dist_sgn(eng_sgn) != static_cast<std::uint32_t>(UINT8_C(0))) };

            return (next_positive ? val : -val); // LCOV_EXCL_LINE
          }
          // LCOV_EXCL_STOP
        }
      };

    dbl_float_a_vec.at(index) = dbl_float_type { gen(true, dbl_float_type { 11 } / 10) };
    dbl_float_b_vec.at(index) = dbl_float_type { gen(true, dbl_float_type { 11 } / 10) };

    dec_float_a_vec.at(index) = dec_float_type { dbl_float_a_vec.at(index) };
    dec_float_b_vec.at(index) = dec_float_type { dbl_float_b_vec.at(index) };

    bin_float_a_vec.at(index) = bin_float_type { dbl_float_a_vec.at(index) };
    bin_float_b_vec.at(index) = bin_float_type { dbl_float_b_vec.at(index) };

    #if defined(BOOST_HAS_FLOAT128)
    flt_float_a_vec.at(index) = flt_float_type { dbl_float_a_vec.at(index) };
    flt_float_b_vec.at(index) = flt_float_type { dbl_float_b_vec.at(index) };
    #endif
  }

  std::vector<dbl_float_type> dbl_float_c_vec (dbl_float_a_vec.size());
  std::vector<dec_float_type> dec_float_c_vec (dbl_float_a_vec.size());
  std::vector<bin_float_type> bin_float_c_vec (dbl_float_a_vec.size());
  #if defined(BOOST_HAS_FLOAT128)
  std::vector<flt_float_type> flt_float_c_vec (dbl_float_a_vec.size());
  #endif

  using stopwatch_type = concurrency::stopwatch;

  stopwatch_type my_stopwatch { };

  for(std::size_t count { UINT8_C(0) }; count < trials; ++count)
  {
    dbl_float_c_vec.at(count) = boost::math::cyl_bessel_j(dbl_float_a_vec.at(count), dbl_float_b_vec.at(count));
  }

  const double elapsed_dbl { stopwatch_type::elapsed_time<double>(my_stopwatch) };

  my_stopwatch.reset();

  for(std::size_t count { UINT8_C(0) }; count < trials; ++count)
  {
    dec_float_c_vec.at(count) = boost::math::cyl_bessel_j(dec_float_a_vec.at(count), dec_float_b_vec.at(count));
  }

  const double elapsed_dec { stopwatch_type::elapsed_time<double>(my_stopwatch) };

  my_stopwatch.reset();

  for(std::size_t count { UINT8_C(0) }; count < trials; ++count)
  {
    bin_float_c_vec.at(count) = boost::math::cyl_bessel_j(bin_float_a_vec.at(count), bin_float_b_vec.at(count));
  }

  const double elapsed_bin { stopwatch_type::elapsed_time<double>(my_stopwatch) };

  #if defined(BOOST_HAS_FLOAT128)
  my_stopwatch.reset();

  for(std::size_t count { UINT8_C(0) }; count < trials; ++count)
  {
    flt_float_c_vec.at(count) = boost::math::cyl_bessel_j(flt_float_a_vec.at(count), flt_float_b_vec.at(count));
  }

  const double elapsed_flt { stopwatch_type::elapsed_time<double>(my_stopwatch) };
  #endif

  std::stringstream strm { };

  strm << std::fixed << std::setprecision(3) << "elapsed_dbl     : " << elapsed_dbl << "s\n"
       << std::fixed << std::setprecision(3) << "elapsed_dec     : " << elapsed_dec << "s\n"
       << std::fixed << std::setprecision(3) << "elapsed_bin     : " << elapsed_bin << "s\n"
  #if defined(BOOST_HAS_FLOAT128)
       << std::fixed << std::setprecision(3) << "elapsed_flt     : " << elapsed_flt << "s\n"
  #endif
       << std::fixed << std::setprecision(3) << "ratio (dec/dbl) : " << elapsed_dec / elapsed_dbl << "\n"
       << std::fixed << std::setprecision(3) << "ratio (bin/dbl) : " << elapsed_bin / elapsed_dbl << "\n"
  #if defined(BOOST_HAS_FLOAT128)
       << std::fixed << std::setprecision(3) << "ratio (flt/dbl) : " << elapsed_flt / elapsed_dbl << "\n"
  #endif
    ;

  BOOST_TEST(elapsed_dec / elapsed_dbl > 1.0);
  BOOST_TEST(elapsed_bin / elapsed_dbl > 1.0);

  std::cout << strm.str() << std::endl;

  std::cout << "verifying results...\n";

  std::size_t count { UINT8_C(0) };

  constexpr dbl_float_type tol_dbl { std::numeric_limits<dbl_float_type>::epsilon() * 0x1000000 };

  for(const auto& lhs : dbl_float_c_vec)
  {
    const dbl_float_type ctrl_dec { dec_float_c_vec[count] };
    const dbl_float_type ctrl_bin { bin_float_c_vec[count] };
    #if defined(BOOST_HAS_FLOAT128)
    const dbl_float_type ctrl_flt { flt_float_c_vec[count] };
    #endif

    BOOST_TEST(is_close_fraction(lhs, ctrl_dec, tol_dbl));
    BOOST_TEST(is_close_fraction(lhs, ctrl_bin, tol_dbl));
    #if defined(BOOST_HAS_FLOAT128)
    BOOST_TEST(is_close_fraction(lhs, ctrl_flt, tol_dbl));
    #endif

    ++count;
  }
}

auto main() -> int
{
  constexpr std::size_t trials { UINT32_C(0x4000) };
  constexpr std::size_t heats { UINT32_C(0x4) };

  for(std::size_t heat_count { UINT8_C(0) }; heat_count < heats; ++heat_count)
  {
    do_trials(trials);
  }

  return boost::report_errors();
}
