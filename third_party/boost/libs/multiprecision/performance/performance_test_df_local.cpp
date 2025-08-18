///////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2024 - 2025.
//  Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt

// This version of "performance_test_df_local.cpp" is intended
// for specific function-by-function comparisons with cpp_double_double.

#include <boost/core/lightweight_test.hpp>
#include <boost/math/special_functions/bessel.hpp>
#include <boost/multiprecision/cpp_double_fp.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#if defined(BOOST_HAS_FLOAT128)
#include <boost/multiprecision/float128.hpp>
#endif

#include <charconv>
#include <cstdint>
#include <ctime>
#include <limits>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <vector>

#if defined(_MSC_VER) && !defined(__GNUC__)
#define STOPWATCH_NODISCARD
#else
#if (defined(__cplusplus) && (__cplusplus >= 201703L))
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
  static auto elapsed_time(const stopwatch& my_stopwatch) noexcept -> RepresentationRequestedTimeType
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

    const time_point_type
      elapsed_ns
      {
        static_cast<time_point_type>
        (
          stop - m_start
        )
      };

    #endif

    return elapsed_ns;
  }
};

} // namespace concurrency

namespace util {

template<typename UnsignedIntegerType,
         const std::uint_fast8_t BaseRepresentation = static_cast<std::uint_fast8_t>(UINT8_C(10)),
         const bool UpperCase = true>
auto baselexical_cast(const UnsignedIntegerType& u, char* first, char* last) -> const char*
{
  constexpr auto my_base = static_cast<int>(BaseRepresentation);

  const auto result = std::to_chars(first, last, u, my_base);

  return result.ptr;
}

template<typename UnsignedIntegralType>
auto util_pseudorandom_time_point_seed() -> UnsignedIntegralType
{
  using stopwatch_type = concurrency::stopwatch;

  return static_cast<UnsignedIntegralType>(stopwatch_type::now());
}

} // namespace util

std::uniform_int_distribution<std::uint32_t> dist_sgn(UINT32_C(   0), UINT32_C(    1)); // NOLINT(cert-err58-cpp,cppcoreguidelines-avoid-non-const-global-variables)
std::uniform_int_distribution<std::uint32_t> dist_dig(UINT32_C(0x31), UINT32_C( 0x39)); // NOLINT(cert-err58-cpp,cppcoreguidelines-avoid-non-const-global-variables)

using eng_sgn_type = std::ranlux24;
using eng_dig_type = std::minstd_rand0;
using eng_exp_type = std::mt19937;

eng_sgn_type eng_sgn; // NOLINT(cert-msc32-c,cert-msc51-cpp,cert-err58-cpp,cppcoreguidelines-avoid-non-const-global-variables)
eng_dig_type eng_dig; // NOLINT(cert-msc32-c,cert-msc51-cpp,cert-err58-cpp,cppcoreguidelines-avoid-non-const-global-variables)

template<typename FloatingPointTypeWithStringConstruction>
auto generate_wide_decimal_value(bool is_positive     = false,
                                 int  digits10_to_get = std::numeric_limits<FloatingPointTypeWithStringConstruction>::digits10 - 2) -> FloatingPointTypeWithStringConstruction
{
  using local_floating_point_type = FloatingPointTypeWithStringConstruction;

  static_assert(std::numeric_limits<local_floating_point_type>::digits10 > static_cast<int>(INT8_C(9)),
                "Error: Floating-point type destination does not have enough digits10");

  std::string str_x(static_cast<std::size_t>(digits10_to_get), '0');

  std::generate(str_x.begin(),
                str_x.end(),
                []() // NOLINT(modernize-use-trailing-return-type,-warnings-as-errors)
                {
                  return static_cast<char>(dist_dig(eng_dig));
                });

  // Insert a decimal point.
  str_x.insert(static_cast<std::size_t>(UINT8_C(1)), static_cast<std::size_t>(UINT8_C(1)), '.');

  // Insert either a positive sign or a negative sign
  // (always one or the other) depending on the sign of x.
  const auto sign_char_to_insert =
    static_cast<char>
    (
      is_positive
        ? '+'
        : static_cast<char>((dist_sgn(eng_sgn) != static_cast<std::uint32_t>(UINT8_C(0))) ? '+' : '-')
    );

  str_x.insert(static_cast<std::size_t>(UINT8_C(0)), static_cast<std::size_t>(UINT8_C(1)), sign_char_to_insert);

  return local_floating_point_type(str_x.c_str());
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

auto main() -> int
{
  using dbl_float_type = boost::multiprecision::cpp_double_double;
  using dec_float_type = boost::multiprecision::number<boost::multiprecision::cpp_dec_float<31>, boost::multiprecision::et_off>;
  #if defined(BOOST_HAS_FLOAT128)
  using flt_float_type = boost::multiprecision::float128;
  #endif

  constexpr std::size_t trials { UINT32_C(100000) };

  std::vector<dbl_float_type> dbl_float_a_vec (trials);
  std::vector<dbl_float_type> dbl_float_b_vec (dbl_float_a_vec.size());

  std::vector<dec_float_type> dec_float_a_vec (dbl_float_a_vec.size());
  std::vector<dec_float_type> dec_float_b_vec (dbl_float_a_vec.size());
  #if defined(BOOST_HAS_FLOAT128)
  std::vector<flt_float_type> flt_float_a_vec (dbl_float_a_vec.size());
  std::vector<flt_float_type> flt_float_b_vec (dbl_float_a_vec.size());
  #endif

  for(std::size_t index { UINT8_C(0) }; index < dbl_float_a_vec.size(); ++index)
  {
    if(std::size_t { index % unsigned { UINT32_C(0x800) } } == std::size_t { UINT8_C(0) })
    {
      eng_sgn.seed(util::util_pseudorandom_time_point_seed<typename eng_sgn_type::result_type>());
      eng_dig.seed(util::util_pseudorandom_time_point_seed<typename eng_dig_type::result_type>());
    }

    auto
      gen
      {
        [](bool is_positive, const dbl_float_type& gate = 0 )
        {
          dbl_float_type val { };

          if(is_positive && gate != 0)
          {
            while((val = generate_wide_decimal_value<dbl_float_type>(true)) < gate) { ; }
          }
          else
          {
            val = generate_wide_decimal_value<dbl_float_type>(true);
          }

          return val;
        }
      };

    dbl_float_a_vec[index] = dbl_float_type { gen(true, dbl_float_type { 9 } / 8) };
    dbl_float_b_vec[index] = dbl_float_type { gen(true, dbl_float_type { 9 } / 8) };

    dec_float_a_vec[index] = dec_float_type { dbl_float_a_vec[index] };
    dec_float_b_vec[index] = dec_float_type { dbl_float_b_vec[index] };

    #if defined(BOOST_HAS_FLOAT128)
    flt_float_a_vec[index] = flt_float_type { dbl_float_a_vec[index] };
    flt_float_b_vec[index] = flt_float_type { dbl_float_b_vec[index] };
    #endif
  }

  std::vector<dbl_float_type> dbl_float_c_vec (dbl_float_a_vec.size());
  std::vector<dec_float_type> dec_float_c_vec (dbl_float_a_vec.size());
  #if defined(BOOST_HAS_FLOAT128)
  std::vector<flt_float_type> flt_float_c_vec (dbl_float_a_vec.size());
  #endif

  using stopwatch_type = concurrency::stopwatch;

  stopwatch_type my_stopwatch { };

  for(std::size_t count { UINT8_C(0) }; count < trials; ++count)
  {
    dbl_float_c_vec[count] = boost::multiprecision::cyl_bessel_j(dbl_float_a_vec[count], dbl_float_b_vec[count]);
  }

  const double elapsed_dbl { stopwatch_type::elapsed_time<double>(my_stopwatch) };

  my_stopwatch.reset();

  for(std::size_t count { UINT8_C(0) }; count < trials; ++count)
  {
    dec_float_c_vec[count] = boost::multiprecision::cyl_bessel_j(dec_float_a_vec[count], dec_float_b_vec[count]);
  }

  const double elapsed_dec { stopwatch_type::elapsed_time<double>(my_stopwatch) };

  #if defined(BOOST_HAS_FLOAT128)
  my_stopwatch.reset();

  for(std::size_t count { UINT8_C(0) }; count < trials; ++count)
  {
    flt_float_c_vec[count] = boost::multiprecision::cyl_bessel_j(flt_float_a_vec[count], flt_float_b_vec[count]);
  }

  const double elapsed_flt { stopwatch_type::elapsed_time<double>(my_stopwatch) };
  #endif

  std::stringstream strm { };

  strm << std::fixed << std::setprecision(3) << "elapsed_dbl     : " << elapsed_dbl << "s\n"
       << std::fixed << std::setprecision(3) << "elapsed_dec     : " << elapsed_dec << "s\n"
  #if defined(BOOST_HAS_FLOAT128)
       << std::fixed << std::setprecision(3) << "elapsed_flt     : " << elapsed_flt << "s\n"
  #endif
       << std::fixed << std::setprecision(3) << "ratio (dec/dbl) : " << elapsed_dec / elapsed_dbl << "\n"
  #if defined(BOOST_HAS_FLOAT128)
       << std::fixed << std::setprecision(3) << "ratio (flt/dbl) : " << elapsed_flt / elapsed_dbl << "\n"
  #endif
    ;

  std::cout << strm.str() << std::endl;

  std::cout << "verifying results...\n";

  std::size_t count { UINT8_C(0) };

  constexpr dbl_float_type tol_dbl { std::numeric_limits<dbl_float_type>::epsilon() * 1024 };

  for(const auto& lhs : dbl_float_c_vec)
  {
    const dbl_float_type ctrl_dec { dec_float_c_vec[count] };

    BOOST_TEST(is_close_fraction(lhs, ctrl_dec, tol_dbl));

    #if defined(BOOST_HAS_FLOAT128)
    const dbl_float_type ctrl_flt { flt_float_c_vec[count] };

    BOOST_TEST(is_close_fraction(lhs, ctrl_flt, tol_dbl));
    #endif

    ++count;
  }

  return boost::report_errors();
}
