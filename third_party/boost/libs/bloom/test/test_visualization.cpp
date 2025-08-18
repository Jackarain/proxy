/* Copyright 2025 Braden Ganetsky.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See https://www.boost.org/libs/bloom for library home page.
 */

/* This is a file for testing of visualizations,
 * such as Visual Studio Natvis or GDB pretty printers.
 * Run this test and break at the label called `break_here`.
 * Inspect the variables to test correctness.
 */

#include <boost/core/lightweight_test.hpp>

#include <boost/bloom/filter.hpp>

void break_here() {}

// Prevent any "unused" errors
template <class... Args> void use(Args&&...) {}

int main()
{
  boost::bloom::filter<int, 1> filter1{};
  boost::bloom::filter<int, 1> filter2{1};
  boost::bloom::filter<int, 5> filter3{{1,2,3,4,5}, 2000};

  use(filter1);
  use(filter2);
  use(filter3);

  break_here();

  return boost::report_errors();
}
