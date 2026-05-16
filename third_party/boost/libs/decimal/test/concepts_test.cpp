// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/decimal.hpp>
#include <boost/core/lightweight_test.hpp>

/*
 * With concepts we get the following error for the call to hypot
 *
 * ../../../boost/decimal/detail/cmath/hypot.hpp:23:16: note: candidate template ignored: constraints not satisfied [with T1 = boost::decimal::decimal32_t, T2 = double]
   23 | constexpr auto hypot(T1 x, T2 y) noexcept
      |                ^
../../../boost/decimal/detail/cmath/hypot.hpp:22:51: note: because 'double' does not satisfy 'decimal_floating_point_type'
   22 | template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE T1, BOOST_DECIMAL_DECIMAL_FLOATING_TYPE T2>
      |                                                   ^
../../../boost/decimal/detail/concepts.hpp:224:45: note: expanded from macro 'BOOST_DECIMAL_DECIMAL_FLOATING_TYPE'
  224 | #define BOOST_DECIMAL_DECIMAL_FLOATING_TYPE boost::decimal::concepts::decimal_floating_point_type
      |                                             ^
../../../boost/decimal/detail/concepts.hpp:200:39: note: because 'boost::decimal::detail::is_decimal_floating_point_v<double>' evaluated to false
  200 | concept decimal_floating_point_type = boost::decimal::detail::is_decimal_floating_point_v<T>;
      |                                       ^
../../../boost/decimal/detail/cmath/hypot.hpp:63:16: note: candidate function template not viable: requires 3 arguments, but 2 were provided
   63 | constexpr auto hypot(T1 x, T2 y, T3 z) noexcept
  *
  *
  * Without we get endless template spaghetti:
  *
  *
  * In file included from concepts_test.cpp:5:
In file included from ../../../boost/decimal.hpp:12:
In file included from ../../../boost/decimal/cmath.hpp:30:
../../../boost/decimal/detail/cmath/hypot.hpp:29:27: error: no matching function for call to 'isnan'
   29 |     if (abs(x) == zero || isnan(y))
      |                           ^~~~~
concepts_test.cpp:13:19: note: in instantiation of function template specialization 'boost::decimal::hypot<boost::decimal::decimal32_t, double>' requested here
   13 |     BOOST_TEST_EQ(hypot(T(1), 1.0), 1.0);
      |                   ^
concepts_test.cpp:19:5: note: in instantiation of function template specialization 'test<boost::decimal::decimal32_t>' requested here
   19 |     test<boost::decimal::decimal32_t>();
      |     ^
../../../boost/decimal/decimal32_t.hpp:740:16: note: candidate function not viable: no known conversion from 'double' to 'decimal32_t' for 1st argument
  740 | constexpr auto isnan BOOST_DECIMAL_PREVENT_MACRO_SUBSTITUTION (decimal32_t rhs) noexcept -> bool
      |                ^                                               ~~~~~~~~~~~~~
../../../boost/decimal/decimal64_t.hpp:967:16: note: candidate function not viable: no known conversion from 'double' to 'decimal64_t' for 1st argument
  967 | constexpr auto isnan BOOST_DECIMAL_PREVENT_MACRO_SUBSTITUTION (decimal64_t rhs) noexcept -> bool
      |                ^                                               ~~~~~~~~~~~~~
../../../boost/decimal/decimal128_t.hpp:996:16: note: candidate function not viable: no known conversion from 'double' to 'decimal128_t' for 1st argument
  996 | constexpr auto isnan BOOST_DECIMAL_PREVENT_MACRO_SUBSTITUTION (decimal128_t rhs) noexcept -> bool
      |                ^                                               ~~~~~~~~~~~~~~
In file included from concepts_test.cpp:5:
In file included from ../../../boost/decimal.hpp:12:
In file included from ../../../boost/decimal/cmath.hpp:30:
../../../boost/decimal/detail/cmath/hypot.hpp:33:14: error: no matching function for call to 'abs'
   33 |     else if (abs(y) == zero || isnan(x))
      |              ^~~
../../../boost/decimal/detail/cmath/abs.hpp:19:16: note: candidate template ignored: requirement 'detail::is_decimal_floating_point_v<double>' was not satisfied [with T = double]
   19 | constexpr auto abs BOOST_DECIMAL_PREVENT_MACRO_SUBSTITUTION (T rhs) noexcept
      |                ^
In file included from concepts_test.cpp:5:
In file included from ../../../boost/decimal.hpp:12:
In file included from ../../../boost/decimal/cmath.hpp:30:
../../../boost/decimal/detail/cmath/hypot.hpp:37:26: error: no matching function for call to 'isinf'
   37 |     else if (isinf(x) || isinf(y))
      |                          ^~~~~
../../../boost/decimal/decimal32_t.hpp:750:16: note: candidate function not viable: no known conversion from 'double' to 'decimal32_t' for 1st argument
  750 | constexpr auto isinf BOOST_DECIMAL_PREVENT_MACRO_SUBSTITUTION (decimal32_t rhs) noexcept -> bool
      |                ^                                               ~~~~~~~~~~~~~
../../../boost/decimal/decimal64_t.hpp:972:16: note: candidate function not viable: no known conversion from 'double' to 'decimal64_t' for 1st argument
  972 | constexpr auto isinf BOOST_DECIMAL_PREVENT_MACRO_SUBSTITUTION (decimal64_t rhs) noexcept -> bool
      |                ^                                               ~~~~~~~~~~~~~
../../../boost/decimal/decimal128_t.hpp:1001:16: note: candidate function not viable: no known conversion from 'double' to 'decimal128_t' for 1st argument
 1001 | constexpr auto isinf BOOST_DECIMAL_PREVENT_MACRO_SUBSTITUTION (decimal128_t rhs) noexcept -> bool
      |                ^                                               ~~~~~~~~~~~~~~
In file included from concepts_test.cpp:5:
In file included from ../../../boost/decimal.hpp:12:
In file included from ../../../boost/decimal/cmath.hpp:30:
../../../boost/decimal/detail/cmath/hypot.hpp:44:44: error: no matching function for call to 'abs'
   44 |     auto new_y {static_cast<promoted_type>(abs(y))};
      |                                            ^~~
../../../boost/decimal/detail/cmath/abs.hpp:19:16: note: candidate template ignored: requirement 'detail::is_decimal_floating_point_v<double>' was not satisfied [with T = double]
   19 | constexpr auto abs BOOST_DECIMAL_PREVENT_MACRO_SUBSTITUTION (T rhs) noexcept
      |                ^
In file included from concepts_test.cpp:6:
../../../boost/core/lightweight_test.hpp:217:62: error: invalid operands to binary expression ('const boost::decimal::decimal32_t' and 'const double')
  217 |     bool operator()(const T& t, const U& u) const { return t == u; }
      |                                                            ~ ^  ~
../../../boost/core/lightweight_test.hpp:294:9: note: in instantiation of function template specialization 'boost::detail::lw_test_eq::operator()<boost::decimal::decimal32_t, double>' requested here
  294 |     if( pred(t, u) )
      |         ^
concepts_test.cpp:13:5: note: in instantiation of function template specialization 'boost::detail::test_with_impl<boost::detail::lw_test_eq, boost::decimal::decimal32_t, double>' requested here
   13 |     BOOST_TEST_EQ(hypot(T(1), 1.0), 1.0);
      |     ^
../../../boost/core/lightweight_test.hpp:539:55: note: expanded from macro 'BOOST_TEST_EQ'
  539 | #define BOOST_TEST_EQ(expr1,expr2) ( ::boost::detail::test_with_impl(::boost::detail::lw_test_eq(), #expr1, #expr2, __FILE__, __LINE__, BOOST_CURRENT_FUNCTION, expr1, expr2) )
      |                                                       ^
concepts_test.cpp:19:5: note: in instantiation of function template specialization 'test<boost::decimal::decimal32_t>' requested here
   19 |     test<boost::decimal::decimal32_t>();
      |     ^
../../../boost/decimal/decimal32_t.hpp:1214:16: note: candidate function not viable: no known conversion from 'const double' to 'decimal32_t' for 2nd argument
 1214 | constexpr auto operator==(decimal32_t lhs, decimal32_t rhs) noexcept -> bool
      |                ^                         ~~~~~~~~~~~~~
../../../boost/decimal/decimal32_t.hpp:1232:16: note: candidate function template not viable: no known conversion from 'const double' to 'decimal32_t' for 2nd argument
 1232 | constexpr auto operator==(Integer lhs, decimal32_t rhs) noexcept -> std::enable_if_t<detail::is_integral_v<Integer>, bool>
      |                ^                       ~~~~~~~~~~~~~
../../../boost/decimal/decimal64_t.hpp:1824:16: note: candidate function template not viable: no known conversion from 'const double' to 'decimal64_t' for 2nd argument
 1824 | constexpr auto operator==(Integer lhs, decimal64_t rhs) noexcept
      |                ^                       ~~~~~~~~~~~~~
../../../boost/decimal/decimal128_t.hpp:1057:16: note: candidate function template not viable: no known conversion from 'const double' to 'decimal128_t' for 2nd argument
 1057 | constexpr auto operator==(Integer lhs, decimal128_t rhs) noexcept
      |                ^                       ~~~~~~~~~~~~~~
../../../boost/decimal/decimal64_t.hpp:1804:16: note: candidate function not viable: no known conversion from 'const double' to 'decimal64_t' for 2nd argument
 1804 | constexpr auto operator==(decimal64_t lhs, decimal64_t rhs) noexcept -> bool
      |                ^                         ~~~~~~~~~~~~~
../../../boost/decimal/decimal128_t.hpp:1037:16: note: candidate function not viable: no known conversion from 'const double' to 'decimal128_t' for 2nd argument
 1037 | constexpr auto operator==(decimal128_t lhs, decimal128_t rhs) noexcept -> bool
      |                ^                          ~~~~~~~~~~~~~~
../../../boost/decimal/detail/comparison.hpp:92:16: note: candidate template ignored: requirement 'detail::is_decimal_floating_point_v<double>' was not satisfied [with Decimal1 = boost::decimal::decimal32_t, Decimal2 = double]
   92 | constexpr auto operator==(Decimal1 lhs, Decimal2 rhs) noexcept
      |                ^
../../../boost/decimal/decimal32_t.hpp:1226:16: note: candidate template ignored: requirement 'detail::is_integral_v<double>' was not satisfied [with Integer = double]
 1226 | constexpr auto operator==(decimal32_t lhs, Integer rhs) noexcept -> std::enable_if_t<detail::is_integral_v<Integer>, bool>
      |                ^
../../../boost/decimal/decimal64_t.hpp:1817:16: note: candidate template ignored: requirement 'detail::is_integral_v<double>' was not satisfied [with Integer = double]
 1817 | constexpr auto operator==(decimal64_t lhs, Integer rhs) noexcept
      |                ^
../../../boost/decimal/decimal128_t.hpp:1050:16: note: candidate template ignored: requirement 'detail::is_integral_v<double>' was not satisfied [with Integer = double]
 1050 | constexpr auto operator==(decimal128_t lhs, Integer rhs) noexcept
 */

template <typename T>
void test()
{
    T res = T(1.0) + 1.0;
    BOOST_TEST_EQ(res, T(2.0));
    BOOST_TEST_EQ(hypot(T(1), 1.0), 1.0);
}

int main()
{
    test<boost::decimal::decimal32_t>();

    return boost::report_errors();
}

