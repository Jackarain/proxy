/* Copyright 2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See https://www.boost.org/libs/bloom for library home page.
 */

#include <boost/bloom.hpp>
#include <boost/core/lightweight_test.hpp>

struct use_types
{
  using type1=boost::bloom::filter<int,1>;
  using type2=boost::bloom::block<unsigned char,1>;
  using type3=boost::bloom::multiblock<unsigned char,1>;
  using type4=boost::bloom::fast_multiblock32<1>;
  using type5=boost::bloom::fast_multiblock64<1>;
};

int main()
{
  (void)use_types{};
  return boost::report_errors();
}
