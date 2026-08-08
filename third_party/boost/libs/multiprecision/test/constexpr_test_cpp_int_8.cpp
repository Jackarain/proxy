//  (C) Copyright Marco Ribeiro 2026.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "constexpr_arithmetric_test.hpp"
#include "boost/multiprecision/cpp_int.hpp"
#include "test.hpp"

#if !defined(BOOST_MP_NO_CONSTEXPR_DETECTION) && !defined(DISABLE_TESTS)

int main()
{
   namespace mp = boost::multiprecision;

   using int512_et_off = mp::number<mp::cpp_int_backend<
      512, 512, mp::signed_magnitude, mp::unchecked, void
   >, mp::et_off>;

   using int512_et_on = mp::number<mp::cpp_int_backend<
      512, 512, mp::signed_magnitude, mp::unchecked, void
   >, mp::et_on>;

   static_assert(mp::abs(int512_et_off(-12345)) == 12345);
   static_assert(mp::conj(int512_et_off(100)) == 100);
   static_assert(mp::proj(int512_et_off(100)) == 100);
   static_assert(mp::powm(int512_et_off(-5), 2, 7) == 4);

   static_assert(mp::abs(int512_et_on(-12345)) == 12345);
   static_assert(mp::conj(int512_et_on(100)) == 100);
   static_assert(mp::proj(int512_et_on(100)) == 100);
   static_assert(mp::powm(int512_et_on(-5), 2, 7) == 4);
}
#else
int main() {}
#endif

