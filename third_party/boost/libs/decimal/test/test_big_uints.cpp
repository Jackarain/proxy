// Copyright 2024 Christopher Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/decimal.hpp>

#if defined(__MINGW32__) || defined(BOOST_DECIMAL_DISABLE_EXCEPTIONS)

// TODO(ckormanyos) disable -Werror in BJAM specifically for this file, as it gets
// a hard-coded, non-removable warning (-Werror) propagated up from Multiprecision.
#include <boost/core/lightweight_test.hpp>

int main()
{
  BOOST_TEST(true);

  return boost::report_errors();
}

#else

// Propagates up from boost.math
#define _SILENCE_CXX23_DENORM_DEPRECATION_WARNING

#include <boost/decimal/detail/u256.hpp>

#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wold-style-cast"
#  pragma clang diagnostic ignored "-Wundef"
#  pragma clang diagnostic ignored "-Wconversion"
#  pragma clang diagnostic ignored "-Wsign-conversion"
#  pragma clang diagnostic ignored "-Wfloat-equal"
#  pragma clang diagnostic ignored "-Wdeprecated-declarations"

#  if __clang_major__ >= 20
#    pragma clang diagnostic ignored "-Wdeprecated-literal-operator"
#    pragma clang diagnostic ignored "-Wfortify-source"
#  endif

#elif defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wold-style-cast"
#  pragma GCC diagnostic ignored "-Wundef"
#  pragma GCC diagnostic ignored "-Wconversion"
#  pragma GCC diagnostic ignored "-Wsign-conversion"
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#  pragma GCC diagnostic ignored "-Wuseless-cast"
#endif

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic ignored "-Wduplicated-branches"
#endif

#include <boost/multiprecision/cpp_int.hpp>
#include <boost/core/lightweight_test.hpp>

#include <array>
#include <chrono>
#include <random>
#include <sstream>
#include <string>

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

  template<typename BigUintType>
  auto declexical_cast(const BigUintType& big_uint) -> std::string
  {
    std::stringstream strm;

    strm << std::dec << big_uint;

    return strm.str();
  }

  template <typename T,
            const std::size_t N>
  auto generate_p10_array() noexcept -> std::array<T, N>
  {
    std::array<T, N> values { };

    std::size_t index { 0 };

    values[index] = T { 1 };

    ++index;

    for ( ; index < N; ++index)
    {
      values[index] = values[index - 1] * UINT64_C(10);
    }

    return values;
  }

} // namespace local

template<typename BoostCtrlUint_Type,
         typename DecInternUint_Type>
auto test_big_uints_mul() -> void
{
  using boost_ctrl_uint_type = BoostCtrlUint_Type;
  using dec_intern_uint_type = DecInternUint_Type;

  constexpr auto digits2 = std::numeric_limits<boost_ctrl_uint_type>::digits;

  using random_engine_type = std::mt19937_64;
  using bit_distribution_type = std::uniform_int_distribution<int>;

  random_engine_type    rng(local::time_point<typename random_engine_type::result_type>());
  bit_distribution_type bts(0, 1);

  bool lhs_is_fixed_and_near_max = true;
  bool rhs_is_fixed_and_variable = (!lhs_is_fixed_and_near_max);

  for(auto trials = static_cast<int>(INT8_C(0)); trials < static_cast<int>(INT16_C(0x100)); ++trials)
  {
    for(auto digits_split  = static_cast<int>(INT8_C(98));
             digits_split  > static_cast<int>(INT8_C(50));
             digits_split -= static_cast<int>(INT8_C( 5)))
    {
      const auto split = static_cast<float>(static_cast<float>(digits_split) / 100.0F);

      auto digits_lhs = static_cast<int>(digits2 - 1);
      auto digits_rhs = static_cast<int>(static_cast<float>((1.0F - split) * digits2));

      boost_ctrl_uint_type boost_ctrl_uint_lhs(1);
      dec_intern_uint_type dec_intern_uint_lhs(1U);
      boost_ctrl_uint_type boost_ctrl_uint_rhs(1);
      dec_intern_uint_type dec_intern_uint_rhs(1U);

      if(lhs_is_fixed_and_near_max)
      {
        boost_ctrl_uint_lhs <<= static_cast<unsigned>(digits2 - 1);
        dec_intern_uint_lhs <<= static_cast<unsigned>(digits2 - 1);
      }
      else
      {
        for(int i = 1; i < digits_lhs; ++i) { const int next_bit = bts(rng); boost_ctrl_uint_lhs <<= 1; dec_intern_uint_lhs <<= 1; if(next_bit != 0) { boost_ctrl_uint_lhs |= boost_ctrl_uint_type(1); dec_intern_uint_lhs |= dec_intern_uint_type(1); } }
      }

      if(rhs_is_fixed_and_variable)
      {
        boost_ctrl_uint_rhs <<= static_cast<unsigned>(digits_rhs);
        dec_intern_uint_rhs <<= static_cast<unsigned>(digits_rhs);
      }
      else
      {
        for(int i = 1; i < digits_rhs; ++i) { const int next_bit = bts(rng); boost_ctrl_uint_rhs <<= 1; dec_intern_uint_rhs <<= 1; if(next_bit != 0) { boost_ctrl_uint_rhs |= boost_ctrl_uint_type(1); dec_intern_uint_rhs |= dec_intern_uint_type(1); } }
      }

      lhs_is_fixed_and_near_max = (!lhs_is_fixed_and_near_max);
      rhs_is_fixed_and_variable = (!rhs_is_fixed_and_variable);

      if(digits_rhs >= 64)
      {
        const auto dec_intern_mul = dec_intern_uint_lhs * dec_intern_uint_rhs;
        const auto boost_ctrl_mul = boost_ctrl_uint_lhs * boost_ctrl_uint_rhs;

        BOOST_TEST(local::declexical_cast(dec_intern_mul) == local::declexical_cast(boost_ctrl_mul));
      }
      else
      {
        const auto dec_intern_rhs_64 =
          static_cast<std::uint64_t>
          (
            static_cast<boost::int128::uint128_t>(dec_intern_uint_rhs)
          );

        const auto boost_ctrl_rhs_64 = static_cast<std::uint64_t>(boost_ctrl_uint_rhs);

        BOOST_TEST_EQ(dec_intern_rhs_64, boost_ctrl_rhs_64);

        const std::uint64_t rhs_64 = dec_intern_rhs_64;

        const auto dec_intern_mul = dec_intern_uint_lhs * rhs_64;
        const auto boost_ctrl_mul = boost_ctrl_uint_lhs * rhs_64;

        const auto str_dec_intern_mul { local::declexical_cast(dec_intern_mul) };
        const auto str_boost_ctrl_mul { local::declexical_cast(boost_ctrl_mul) };

        BOOST_TEST_EQ(str_dec_intern_mul, str_boost_ctrl_mul);
      }
    }
  }
}

template<typename BoostCtrlUint_Type,
         typename DecInternUint_Type>
auto test_big_uints_div() -> void
{
  using boost_ctrl_uint_type = BoostCtrlUint_Type;
  using dec_intern_uint_type = DecInternUint_Type;

  constexpr auto digits2 = std::numeric_limits<boost_ctrl_uint_type>::digits;

  using random_engine_type = std::mt19937_64;
  using bit_distribution_type = std::uniform_int_distribution<int>;

  random_engine_type    rng(local::time_point<typename random_engine_type::result_type>());
  bit_distribution_type bts(0, 1);

  bool lhs_is_fixed_and_near_max = true;
  bool rhs_is_fixed_and_variable = (!lhs_is_fixed_and_near_max);

  for(auto trials = static_cast<int>(INT8_C(0)); trials < static_cast<int>(INT16_C(0x100)); ++trials)
  {
    for(auto digits_split  = static_cast<int>(INT8_C(98));
             digits_split  > static_cast<int>(INT8_C(50));
             digits_split -= static_cast<int>(INT8_C( 5)))
    {
      const auto split = static_cast<float>(static_cast<float>(digits_split) / 100.0F);

      auto digits_lhs = static_cast<int>(digits2 - 1);
      auto digits_rhs = static_cast<int>(static_cast<float>((1.0F - split) * digits2));

      boost_ctrl_uint_type boost_ctrl_uint_lhs(1);
      dec_intern_uint_type dec_intern_uint_lhs(1);
      boost_ctrl_uint_type boost_ctrl_uint_rhs(1);
      dec_intern_uint_type dec_intern_uint_rhs(1);

      if(lhs_is_fixed_and_near_max)
      {
        boost_ctrl_uint_lhs <<= static_cast<unsigned>(digits2 - 1);
        dec_intern_uint_lhs <<= static_cast<unsigned>(digits2 - 1);
      }
      else
      {
        for(int i = 1; i < digits_lhs; ++i) { const int next_bit = bts(rng); boost_ctrl_uint_lhs <<= 1; dec_intern_uint_lhs <<= 1; if(next_bit != 0) { boost_ctrl_uint_lhs |= boost_ctrl_uint_type(1); dec_intern_uint_lhs |= dec_intern_uint_type(1); } }
      }

      if(rhs_is_fixed_and_variable)
      {
        boost_ctrl_uint_rhs <<= static_cast<unsigned>(digits_rhs);
        dec_intern_uint_rhs <<= static_cast<unsigned>(digits_rhs);
      }
      else
      {
        for(int i = 1; i < digits_rhs; ++i) { const int next_bit = bts(rng); boost_ctrl_uint_rhs <<= 1; dec_intern_uint_rhs <<= 1; if(next_bit != 0) { boost_ctrl_uint_rhs |= boost_ctrl_uint_type(1); dec_intern_uint_rhs |= dec_intern_uint_type(1); } }
      }

      lhs_is_fixed_and_near_max = (!lhs_is_fixed_and_near_max);
      rhs_is_fixed_and_variable = (!rhs_is_fixed_and_variable);

      const auto dec_intern_div = dec_intern_uint_lhs / dec_intern_uint_rhs;
      const auto boost_ctrl_div = boost_ctrl_uint_lhs / boost_ctrl_uint_rhs;

      BOOST_TEST(local::declexical_cast(dec_intern_div) == local::declexical_cast(boost_ctrl_div));
    }
  }
}

auto test_various_spots() -> void
{
  using local_uint128_type = boost::int128::uint128_t;

  std::uniform_int_distribution<std::uint64_t>
    lower_dist
    (
      UINT64_C(0xFFFFFFFFFFFFFFF8),
      (std::numeric_limits<std::uint64_t>::max)()
    );

  using random_engine_type = std::mt19937_64;

  random_engine_type rng(local::time_point<typename random_engine_type::result_type>());

  for(auto trials = static_cast<int>(INT8_C(0)); trials < static_cast<int>(INT8_C(0x8)); ++trials)
  {
    static_cast<void>(trials);

    std::uint64_t lowest_low { (std::numeric_limits<std::uint64_t>::max)() };

    local_uint128_type low { UINT64_C(0), lower_dist(rng) };

    for(auto idx = static_cast<int>(INT8_C(0)); idx < static_cast<int>(INT8_C(0x10)); ++idx)
    {
      static_cast<void>(idx);

      const local_uint128_type low_old { low };

      ++low;

      const std::uint64_t new_low { static_cast<std::uint64_t>(low) };

      if(new_low < lowest_low)
      {
        lowest_low = new_low;
      }

      BOOST_TEST(low > low_old);
    }

    BOOST_TEST_EQ(lowest_low, UINT64_C(0));
  }

  std::uniform_int_distribution<unsigned> n_dist(1, 4);

  for(auto trials = static_cast<int>(INT8_C(0)); trials < static_cast<int>(INT8_C(0x8)); ++trials)
  {
    static_cast<void>(trials);

    local_uint128_type low { UINT64_C(0), lower_dist(rng) };

    for(auto idx = static_cast<int>(INT8_C(0)); idx < static_cast<int>(INT8_C(0x10)); ++idx)
    {
      static_cast<void>(idx);

      const local_uint128_type low_old { low };

      const auto n_add = n_dist(rng);

      low += static_cast<std::uint64_t>(n_add);

      BOOST_TEST((low > low_old) && ((low - n_add) == low_old));
    }
  }
}

template <typename T>
auto test_spot_div_uint256_t() -> void
{
  using boost_ctrl_uint_type = boost::multiprecision::uint256_t;
  using dec_intern_uint_type = T;

  {
    // Specially test several exactly-curated division operations that are know
    // to cover the hard-to-hit carry/borrow line(s) in Knuth long-division.

    using dec_intern_array_type = std::array<dec_intern_uint_type, static_cast<std::size_t>(UINT8_C(3))>;
    using boost_ctrl_array_type = std::array<boost_ctrl_uint_type, static_cast<std::size_t>(UINT8_C(3))>;

    dec_intern_array_type dec_intern_top_list =
    {{
      dec_intern_uint_type { { UINT64_C(0x01AC01E281D83F28), UINT64_C(0x698C19FD72AA8085) }, { UINT64_C(0x78F0CD3B0CD2FF5D), UINT64_C(0xD6A0A4DB3233D019) } },
      dec_intern_uint_type { { UINT64_C(0x009E1F4B3859275E), UINT64_C(0xE297AFBAB4ADB30B) }, { UINT64_C(0xFAE7A9D4CAF5672E), UINT64_C(0xB279A59B9906070C) } },
      dec_intern_uint_type { { UINT64_C(0x000B6F4866E326CC), UINT64_C(0x1321EAE5369D68E5) }, { UINT64_C(0x824E7315340514AB), UINT64_C(0x6EF6D107ECB8BC38) } }
    }};

    dec_intern_array_type dec_intern_bot_list =
    {{
      dec_intern_uint_type { { UINT64_C(0x0000000000000000), UINT64_C(0x0000000000000000) }, { UINT64_C(0x292FFA3C03F252D4), UINT64_C(0x42D1483A455B4281) } },
      dec_intern_uint_type { { UINT64_C(0x0000000000000000), UINT64_C(0x0000000000000000) }, { UINT64_C(0x2AA34E5021771CBC), UINT64_C(0x4EB1EFC17289FA09) } },
      dec_intern_uint_type { { UINT64_C(0x0000000000000000), UINT64_C(0x0000000000000000) }, { UINT64_C(0x718E8C8F6DBB6F76), UINT64_C(0x597BD68B19ACF237) } }
    }};

    boost_ctrl_array_type boost_ctrl_top_list =
    {{
      boost_ctrl_uint_type("0x1AC01E281D83F28698C19FD72AA808578F0CD3B0CD2FF5DD6A0A4DB3233D019"),
      boost_ctrl_uint_type("0x9E1F4B3859275EE297AFBAB4ADB30BFAE7A9D4CAF5672EB279A59B9906070C"),
      boost_ctrl_uint_type("0xB6F4866E326CC1321EAE5369D68E5824E7315340514AB6EF6D107ECB8BC38")
    }};

    boost_ctrl_array_type boost_ctrl_bot_list =
    {{
      boost_ctrl_uint_type("0x292FFA3C03F252D442D1483A455B4281"),
      boost_ctrl_uint_type("0x2AA34E5021771CBC4EB1EFC17289FA09"),
      boost_ctrl_uint_type("0x718E8C8F6DBB6F76597BD68B19ACF237")
    }};

    // Run the cureated test divisions. Verify the answers, and get
    // at least 3 hits on the hard-toreach lines.
    for(auto   index = static_cast<std::size_t>(UINT8_C(0));
               index < std::tuple_size<dec_intern_array_type>::value;
             ++index)
    {
      const auto dec_intern_div = dec_intern_top_list[index] / dec_intern_bot_list[index];
      const auto boost_ctrl_div = boost_ctrl_top_list[index] / boost_ctrl_bot_list[index];

      BOOST_TEST(local::declexical_cast(dec_intern_div) == local::declexical_cast(boost_ctrl_div));
    }

    // And while we are at it, ... ensure that a/a = 1 for these random numerators.
    for(auto   index = static_cast<std::size_t>(UINT8_C(0));
               index < std::tuple_size<dec_intern_array_type>::value;
             ++index)
    {
      const auto dec_intern_div_unity = dec_intern_top_list[index] / dec_intern_top_list[index];
      const auto boost_ctrl_div_unity = boost_ctrl_top_list[index] / boost_ctrl_top_list[index];

      BOOST_TEST(local::declexical_cast(dec_intern_div_unity) == local::declexical_cast(boost_ctrl_div_unity));
      BOOST_TEST(dec_intern_div_unity == 1);
    }
  }
}

template <typename T>
auto test_p10_mul_uint256_t() -> void
{
  using local_uint256_t = T;

  auto powers_of_10 = local::generate_p10_array<local_uint256_t, static_cast<std::size_t>(UINT8_C(78))>();

  std::string str_p10 { "1" };

  for(const auto& ui_val : powers_of_10)
  {
    std::stringstream strm;

    strm << ui_val;

    BOOST_TEST(strm.str() == str_p10);

    str_p10.push_back('0');
  }
}

template<typename BoostCtrlUint_Type,
         typename DecInternUint_Type>
auto test_big_uints_shl() -> void
{
  using boost_ctrl_uint_type = BoostCtrlUint_Type;
  using dec_intern_uint_type = DecInternUint_Type;

  constexpr auto digits2 = std::numeric_limits<boost_ctrl_uint_type>::digits;

  using random_engine_type = std::mt19937_64;
  using bit_distribution_type = std::uniform_int_distribution<int>;

  random_engine_type    rng(local::time_point<typename random_engine_type::result_type>());
  bit_distribution_type bts(0, 1);

  for(auto trials = static_cast<int>(INT8_C(0)); trials < static_cast<int>(INT16_C(0x100)); ++trials)
  {
    auto digits_val = static_cast<int>(digits2 - 1);

    boost_ctrl_uint_type boost_ctrl_uint_val(1);
    dec_intern_uint_type dec_intern_uint_val(1);

    for(int i = 1; i < digits_val; ++i)
    {
      const int next_bit = bts(rng);
      boost_ctrl_uint_val <<= 1;
      dec_intern_uint_val <<= 1;

      if(next_bit != 0)
      {
        boost_ctrl_uint_val |= boost_ctrl_uint_type(1); dec_intern_uint_val |= dec_intern_uint_type(1);
      }
    }

    for(int i = 0; i < 7; ++i)
    {
      const auto dec_intern_shl = dec_intern_uint_val << i;
      const auto boost_ctrl_shl = boost_ctrl_uint_val << i;

      BOOST_TEST(local::declexical_cast(dec_intern_shl) == local::declexical_cast(boost_ctrl_shl));
    }
  }
}

template <typename T>
void test_digit_counting()
{
    using boost::decimal::detail::num_digits;
    constexpr auto max_power {sizeof(T) == sizeof(std::uint64_t) * 4 ? 77 : 38 };

    T current_power {1};
    int current_digits {1};
    for (int i {}; i <= max_power; ++i)
    {
        BOOST_TEST_EQ(current_power, boost::decimal::detail::pow10(static_cast<T>(i)));
        BOOST_TEST_EQ(num_digits(current_power), current_digits);
        current_power = current_power * UINT64_C(10);
        ++current_digits;
    }
}

int main()
{
  #ifndef __s390x__
  test_big_uints_mul<boost::multiprecision::uint128_t, boost::int128::uint128_t  >();
  test_big_uints_mul<boost::multiprecision::uint256_t, boost::decimal::detail::u256>();

  test_p10_mul_uint256_t<boost::decimal::detail::u256>();

  test_digit_counting<boost::int128::uint128_t>();
  test_digit_counting<boost::decimal::detail::u256>();
  #endif

  test_big_uints_div<boost::multiprecision::uint128_t, boost::int128::uint128_t  >();
  test_big_uints_div<boost::multiprecision::uint256_t, boost::decimal::detail::u256>();

  test_various_spots();

  test_spot_div_uint256_t<boost::decimal::detail::u256>();

  test_big_uints_shl<boost::multiprecision::uint128_t, boost::int128::uint128_t  >();
  test_big_uints_shl<boost::multiprecision::uint256_t, boost::decimal::detail::u256>();

  return boost::report_errors();
}

#endif
