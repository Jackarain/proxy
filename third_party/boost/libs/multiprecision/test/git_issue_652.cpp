// Copyright 2025 Christopher Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/core/lightweight_test.hpp>
#include <boost/multiprecision/cpp_bin_float.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace local {

template<typename FloatType, typename LongIshType>
auto test_max_convert() -> void
{
  using float_type = FloatType;

  using float_array_type = std::array<float_type, std::size_t { UINT8_C(7) }>;

  const float_array_type factors =
  {
    float_type { "0.999" },
    float_type { 1 },
    float_type { "1.011" },
    float_type { "2.022" },
    float_type { "3.033" },
    float_type { "4.044" },
    float_type { "5.055" }
  };

  for(std::size_t i = std::size_t { UINT8_C(0) }; i < std::tuple_size<float_array_type>::value; ++i)
  {
    static_cast<void>(i);

    using long_type = LongIshType;

    const float_type
      flt_val
      {
        float_type { (std::numeric_limits<long_type>::max)() } * factors[i]
      };

    const long_type conversion_result { static_cast<long_type>(flt_val) };

    if(i == std::size_t { UINT8_C(0) })
    {
      BOOST_TEST(conversion_result < (std::numeric_limits<long_type>::max)());
    }
    else
    {
      BOOST_TEST(conversion_result == (std::numeric_limits<long_type>::max)());
    }
  }
}

namespace detail {

template<typename FloatType, typename LongIshType>
auto test_convert_neg_to_ll_types_impl(std::vector<LongIshType>& ll_ctrls) -> void
{
  ll_ctrls.clear();

  using float_type = FloatType;

  using std::ldexp;

  using float_array_type = std::array<float_type, std::size_t { UINT8_C(9) }>;

  const float_array_type factors =
  {
    float_type { -ldexp(float_type { 1 }, -28) },
    float_type { "-1.0E-9" },
    float_type { "-0.999" },
    float_type { -1 },
    float_type { "-1.011" },
    float_type { "-2.022" },
    float_type { "-3.033" },
    float_type { "-4.044" },
    float_type { "-5.055" }
  };

  for(std::size_t i = std::size_t { UINT8_C(0) }; i < factors.size(); ++i)
  {
    static_cast<void>(i);

    using float_type = FloatType;
    using long_type = LongIshType;

    const float_type
      flt_val
      {
        static_cast<float_type>((std::numeric_limits<long_type>::max)()) * factors[i]
      };

    const long_type conversion_result { static_cast<long_type>(flt_val) };

    ll_ctrls.push_back(conversion_result);
  }
}

template<typename FloatType, typename LongIshType>
auto test_convert_pos_to_ll_types_impl(std::vector<LongIshType>& ll_ctrls) -> void
{
  ll_ctrls.clear();

  using float_type = FloatType;

  using std::ldexp;

  using float_array_type = std::array<float_type, std::size_t { UINT8_C(9) }>;

  const float_array_type factors =
  {
    float_type { +ldexp(float_type { 1 }, -28) },
    float_type { "+1.0E-9" },
    float_type { "+0.999" },
    float_type { -1 },
    float_type { "+1.011" },
    float_type { "+2.022" },
    float_type { "+3.033" },
    float_type { "+4.044" },
    float_type { "+5.055" }
  };

  for(std::size_t i = std::size_t { UINT8_C(0) }; i < factors.size(); ++i)
  {
    static_cast<void>(i);

    using float_type = FloatType;
    using long_type = LongIshType;

    const float_type
      flt_val
      {
        static_cast<float_type>((std::numeric_limits<long_type>::max)()) * factors[i]
      };

    const long_type conversion_result { static_cast<long_type>(flt_val) };

    ll_ctrls.push_back(conversion_result);
  }
}

} // namespace detail

auto test_convert_pos_to_ll_types() -> void
{
  std::vector<unsigned long long> ull_ctrls_dec;
  std::vector<unsigned long long> ull_ctrls_bin;

  std::vector<signed long long> sll_ctrls_dec;
  std::vector<signed long long> sll_ctrls_bin;

  detail::test_convert_pos_to_ll_types_impl<::boost::multiprecision::cpp_dec_float_50, unsigned long long>(ull_ctrls_dec);
  detail::test_convert_pos_to_ll_types_impl<::boost::multiprecision::cpp_bin_float_50, unsigned long long>(ull_ctrls_bin);

  detail::test_convert_pos_to_ll_types_impl<::boost::multiprecision::cpp_dec_float_50, signed long long>(sll_ctrls_dec);
  detail::test_convert_pos_to_ll_types_impl<::boost::multiprecision::cpp_bin_float_50, signed long long>(sll_ctrls_bin);

  BOOST_TEST(ull_ctrls_dec.size() == ull_ctrls_bin.size());
  BOOST_TEST(!ull_ctrls_dec.empty());

  for(std::size_t i { std::size_t { UINT8_C(0) } }; i < ull_ctrls_dec.size(); ++i)
  {
    BOOST_TEST(ull_ctrls_dec[i] == ull_ctrls_bin[i]);
  }

  BOOST_TEST(sll_ctrls_dec.size() == sll_ctrls_bin.size());
  BOOST_TEST(!sll_ctrls_dec.empty());

  for(std::size_t i { std::size_t { UINT8_C(0) } }; i < sll_ctrls_dec.size(); ++i)
  {
    BOOST_TEST(sll_ctrls_dec[i] == sll_ctrls_bin[i]);
  }
}

auto test_convert_neg_to_ll_types() -> void
{
  std::vector<unsigned long long> ull_ctrls_dec;
  std::vector<unsigned long long> ull_ctrls_bin;

  std::vector<signed long long> sll_ctrls_dec;
  std::vector<signed long long> sll_ctrls_bin;

  detail::test_convert_neg_to_ll_types_impl<::boost::multiprecision::cpp_dec_float_50, unsigned long long>(ull_ctrls_dec);
  detail::test_convert_neg_to_ll_types_impl<::boost::multiprecision::cpp_bin_float_50, unsigned long long>(ull_ctrls_bin);

  detail::test_convert_neg_to_ll_types_impl<::boost::multiprecision::cpp_dec_float_50, signed long long>(sll_ctrls_dec);
  detail::test_convert_neg_to_ll_types_impl<::boost::multiprecision::cpp_bin_float_50, signed long long>(sll_ctrls_bin);

  BOOST_TEST(ull_ctrls_dec.size() == ull_ctrls_bin.size());
  BOOST_TEST(!ull_ctrls_dec.empty());

  for(std::size_t i { std::size_t { UINT8_C(0) } }; i < ull_ctrls_dec.size(); ++i)
  {
    BOOST_TEST(ull_ctrls_dec[i] == ull_ctrls_bin[i]);
  }

  BOOST_TEST(sll_ctrls_dec.size() == sll_ctrls_bin.size());
  BOOST_TEST(!sll_ctrls_dec.empty());

  for(std::size_t i { std::size_t { UINT8_C(0) } }; i < sll_ctrls_dec.size(); ++i)
  {
    BOOST_TEST(sll_ctrls_dec[i] == sll_ctrls_bin[i]);
  }
}

} // namespace local

auto main() -> int
{
  local::test_max_convert<::boost::multiprecision::cpp_dec_float_50, unsigned long long>();

  local::test_max_convert<::boost::multiprecision::cpp_bin_float_50, unsigned long long>();

  local::test_max_convert<::boost::multiprecision::cpp_dec_float_50, signed long long>();

  local::test_max_convert<::boost::multiprecision::cpp_bin_float_50, signed long long>();

  local::test_convert_neg_to_ll_types();
  local::test_convert_pos_to_ll_types();

  return boost::report_errors();
}
