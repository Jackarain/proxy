///////////////////////////////////////////////////////////////////////////////
//  Copyright 2026 Matt Borland. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See: https://github.com/boostorg/multiprecision/issues/759

#include <complex>
#include <iostream>
#include <limits>
#include <boost/multiprecision/float128.hpp>
#include "test.hpp"

int main()
{
   using REAL = boost::multiprecision::float128;

   const REAL inf_v = std::numeric_limits<REAL>::infinity();

   {
      using std::abs;
      const REAL r = abs(inf_v);
      BOOST_CHECK((boost::multiprecision::isinf)(r));
   }

   {
      using std::abs;
      const std::complex<REAL> z{inf_v, REAL(0)};
      const REAL r = abs(z);
      BOOST_CHECK((boost::multiprecision::isinf)(r));
      BOOST_CHECK(!(boost::multiprecision::isnan)(r));
   }

   {
      using std::abs;
      const std::complex<REAL> z{REAL(0), inf_v};
      const REAL r = abs(z);
      BOOST_CHECK((boost::multiprecision::isinf)(r));
      BOOST_CHECK(!(boost::multiprecision::isnan)(r));
   }

   {
      using std::abs;
      const std::complex<REAL> z{-inf_v, REAL(3)};
      const REAL r = abs(z);
      BOOST_CHECK((boost::multiprecision::isinf)(r));
      BOOST_CHECK(!(boost::multiprecision::isnan)(r));
   }

   {
      // Per IEEE 754: hypot(inf, NaN) = inf.
      using std::abs;
      const REAL nan_v = std::numeric_limits<REAL>::quiet_NaN();
      const std::complex<REAL> z{inf_v, nan_v};
      const REAL r = abs(z);
      BOOST_CHECK((boost::multiprecision::isinf)(r));
   }

   {
      // Sanity check on a finite value: abs(3 + 4i) == 5.
      using std::abs;
      const std::complex<REAL> z{REAL(3), REAL(4)};
      const REAL r = abs(z);
      BOOST_CHECK_EQUAL(r, REAL(5));
   }

   return boost::report_errors();
}
