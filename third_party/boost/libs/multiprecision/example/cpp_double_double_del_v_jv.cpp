///////////////////////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2023 - 2025.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//

#include <iomanip>
#include <iostream>

#include <boost/math/special_functions/bessel.hpp>
#include <boost/multiprecision/cpp_double_fp.hpp>

namespace local
{
  template<typename RealValueType,
           typename RealFunctionType>
  RealValueType derivative(const RealValueType& x,
                           const RealValueType& dx,
                           RealFunctionType real_function)
  {
    using real_value_type = RealValueType;

    // Compute the first derivative of the input function
    // using a three-point central difference rule of O(dx^6).

    const auto dx2 = static_cast<real_value_type>(dx  + dx);
    const auto dx3 = static_cast<real_value_type>(dx2 + dx);

    const auto m1 = static_cast<real_value_type>((  real_function(x + dx)
                                                  - real_function(x - dx))  / 2U);
    const auto m2 = static_cast<real_value_type>((  real_function(x + dx2)
                                                  - real_function(x - dx2)) / 4U);
    const auto m3 = static_cast<real_value_type>((  real_function(x + dx3)
                                                  - real_function(x - dx3)) / 6U);

    const auto fifteen_m1 = static_cast<real_value_type>(m1 * 15U);
    const auto six_m2     = static_cast<real_value_type>(m2 *  6U);
    const auto ten_dx     = static_cast<real_value_type>(dx * 10U);

    return ((fifteen_m1 - six_m2) + m3) / ten_dx;
  }

  template<typename FloatType>
  FloatType cyl_bessel_j_of_v(const FloatType& v)
  {
    const auto x = static_cast<FloatType>(static_cast<FloatType>(34U) / 10U);

    return boost::math::cyl_bessel_j(v, x);
  }
} // namespace local

int main()
{
  using boost::multiprecision::cpp_double_double;

  // D[BesselJ[v, 34/10], v]
  // v := 12/10
  // N[Out[1], 30]
  // 0.439649800900385297241807133820

  using std::ldexp;

  const auto  v = static_cast<cpp_double_double>(static_cast<cpp_double_double>(12U) /     10U); // 1.2
  const auto dv = ldexp(static_cast<cpp_double_double>(1U), -10);                                // 1 / 1024 (approx. 1e-18)

  const auto del_v_jv = local::derivative(v, dv, local::cyl_bessel_j_of_v<cpp_double_double>);

  const auto flg = std::cout.flags();

  std::cout << std::fixed
            << std::setprecision(static_cast<std::streamsize>(std::numeric_limits<double>::max_digits10))
            << static_cast<double>(del_v_jv)
            << std::endl;

  std::cout.flags(flg);
}
