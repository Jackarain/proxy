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

#include "boost/optional/optional_fwd.hpp"
#include "boost/optional/optional.hpp"

void statically_test_basic_instantiations()
{
  boost::optional<int> oN, o1(1);
  swap(oN, o1);

  int i = 1;
  boost::optional<int> rN, ri(i);
  swap(rN, ri);
}
