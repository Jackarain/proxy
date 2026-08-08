// Copyright 2026 Chris Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// See: https://github.com/boostorg/decimal/issues/1384

#include <boost/core/lightweight_test.hpp>

// This simulates upstream inclusion of Boost.Log. At the time of
// hash commit e9951622837f63cf0020b22834ad96b01293fd07, this failed.

// This odd-looking test in CI is intended to ensure that such failures
// no longer occur in the future and that the namespace resolution
// remains unequivocal.

namespace boost {
namespace log {

auto do_something() -> void;

// The presence of this namespace-local subroutine itself conflicted
// (at the time of the issue) with Boost.Log. So this behavior is
// simulated here.

auto do_something() -> void { } // LCOV_EXCL_LINE

} // namespace log
} // namespace boost

#include <boost/decimal/cmath.hpp>

auto main() -> int;

auto main() -> int
{
  boost::log::do_something(); // LCOV_EXCL_LINE

  using local_decimal_type = boost::decimal::decimal64_t;

  const local_decimal_type arg_dec { 123456, -5 };

  // N[ArcCosh[123456/100000], 20]
  const local_decimal_type result1 { acosh(arg_dec) };

  // N[Log[Gamma[123456/100000]], 20]
  const local_decimal_type result2 { lgamma(arg_dec) };

  // N[Zeta[123456/10000], 20]
  const local_decimal_type result3 { riemann_zeta(arg_dec * local_decimal_type { 10.0 }) };

  BOOST_TEST(result1 > local_decimal_type { 0.0 }); // 0.67219624862864254285
  BOOST_TEST(result2 < local_decimal_type { 0.0 }); // -0.094616414889227312801
  BOOST_TEST(result3 > local_decimal_type { 1.0 }); // 1.0001934607133929720

  return boost::report_errors();
}
