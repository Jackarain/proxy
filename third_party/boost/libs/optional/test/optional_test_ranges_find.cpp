// Copyright (C) 2026 Andrzej Krzemienski.
//
// Use, modification, and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/lib/optional for documentation.
//
// You are welcome to contact the author at:
//  akrzemi1@gmail.com


#include "boost/optional/optional.hpp"
#include "boost/core/lightweight_test.hpp"
#include <type_traits>

#ifndef BOOST_NO_CXX20_HDR_RANGES
#include <ranges>
#include <concepts>
#include <iterator>
#include <array>
#include <string>

static_assert(std::equality_comparable<boost::none_t>, "boost::none shall be equality comparable");


template <typename T>
void test_that_you_can_find_none_in_a_range_of_optional(T x, T y)
{
  static_assert(std::equality_comparable_with<boost::optional<T>, boost::none_t>, "boost::none shall satisfy the concept");
  static_assert(std::equality_comparable_with<boost::none_t, boost::optional<T> >, "boost::none shall satisfy the concept");
  static_assert(std::indirect_binary_predicate<std::ranges::equal_to, const boost::optional<T>*, const boost::none_t*>, "boost::none shall satisfy the concept");

  //                                        [0]     [1]     [2]
  std::array<boost::optional<T>, 3> arr = {{ x, boost::none, y }};
  auto it = std::ranges::find(arr, boost::none);
  BOOST_TEST_EQ(std::distance(arr.begin(), it), 1);
}
#endif // BOOST_NO_CXX20_HDR_RANGES


# if (defined __GNUC__) && (!defined BOOST_INTEL_CXX_VERSION) && (!defined __clang__)
#  if (__GNUC__ <= 4)
#   define BOOST_OPTIONAL_TEST_INSUFFICIENT_TYPE_TRAIT_SUPPORT
#  endif
# endif

#ifndef BOOST_OPTIONAL_TEST_INSUFFICIENT_TYPE_TRAIT_SUPPORT
static_assert(std::is_trivially_copyable<boost::none_t>::value, "boost::none shall be trivially copyable");
#endif


void test_that_none_is_equal_to_none()
{
  BOOST_TEST(boost::none == boost::none);
  BOOST_TEST(!(boost::none != boost::none));
  auto None = boost::none;
  BOOST_TEST(None == boost::none);
  BOOST_TEST(!(None != boost::none));
}

int main()
{
#ifndef BOOST_NO_CXX20_HDR_RANGES
  test_that_you_can_find_none_in_a_range_of_optional(1, 2);
  test_that_you_can_find_none_in_a_range_of_optional<std::string>("one", "two");
#endif
  test_that_none_is_equal_to_none();
  return boost::report_errors();
}
