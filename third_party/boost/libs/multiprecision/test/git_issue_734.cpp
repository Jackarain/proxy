// Copyright 2025 Christopher Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/core/lightweight_test.hpp>
#include <boost/multiprecision/cpp_bin_float.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <boost/multiprecision/cpp_double_fp.hpp>

#include <iomanip>
#include <iostream>
#include <sstream>
#include <typeinfo>

namespace local {

template<typename BackendType>
auto my_stringify_via_top_level_number(const std::streamsize strm_size) -> std::string
{
  using float_type = boost::multiprecision::number<BackendType, boost::multiprecision::et_off>;

  #if defined(BOOST_MSVC)
  using streamsize_type = std::streamsize;
  #else
  using streamsize_type = int;
  #endif

  float_type x("0.0000000000000000222222222222222");

  std::stringstream strm { };

  strm << std::setprecision(static_cast<streamsize_type>(strm_size)) << std::fixed;
  strm << x;

  return strm.str();
}

template<typename BackendType>
auto my_stringify_via_str_and_backend_str(const std::streamsize strm_size) -> std::string
{
  using float_type = boost::multiprecision::number<BackendType, boost::multiprecision::et_off>;

  #if defined(BOOST_MSVC)
  using streamsize_type = std::streamsize;
  #else
  using streamsize_type = int;
  #endif

  float_type x("0.0000000000000000222222222222222");

  std::stringstream strm { };

  strm << std::setprecision(static_cast<streamsize_type>(strm_size)) << std::fixed;

  return x.str(strm_size, strm.flags());
}

template<typename BackendType>
auto test() -> void
{
  std::cout << "Testing type of test: " << typeid(BackendType).name() << std::endl;

  BOOST_TEST(my_stringify_via_top_level_number<BackendType>(std::streamsize { INT8_C(0) })  == "0");
  BOOST_TEST(my_stringify_via_top_level_number<BackendType>(std::streamsize { INT8_C(8) })  == "0.00000000");
  BOOST_TEST(my_stringify_via_top_level_number<BackendType>(std::streamsize { INT8_C(12) }) == "0.000000000000");
  BOOST_TEST(my_stringify_via_top_level_number<BackendType>(std::streamsize { INT8_C(31) }) == "0.0000000000000000222222222222222");

  BOOST_TEST(my_stringify_via_str_and_backend_str<BackendType>(std::streamsize { INT8_C(0) })  == "0");
  BOOST_TEST(my_stringify_via_str_and_backend_str<BackendType>(std::streamsize { INT8_C(8) })  == "0.00000000");
  BOOST_TEST(my_stringify_via_str_and_backend_str<BackendType>(std::streamsize { INT8_C(12) }) == "0.000000000000");
  BOOST_TEST(my_stringify_via_str_and_backend_str<BackendType>(std::streamsize { INT8_C(31) }) == "0.0000000000000000222222222222222");
}

} // namespace local

auto main() -> int
{
  local::test<boost::multiprecision::cpp_bin_float<32>>();
  local::test<boost::multiprecision::cpp_dec_float<32>>();
  BOOST_IF_CONSTEXPR(std::numeric_limits<double>::digits >= 53)
  {
    local::test<boost::multiprecision::cpp_double_fp_backend<double>>();
  }

  return boost::report_errors();
}
