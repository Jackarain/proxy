/* Basic example of use of Boost.Bloom.
 * 
 * Copyright 2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See https://www.boost.org/libs/bloom for library home page.
 */

#include <boost/bloom.hpp>
#include <cassert>
#include <iostream>
#include <string>

int main()
{
  /* Bloom filter of strings with 5 bits set per insertion */

  using filter = boost::bloom::filter<std::string, 5>;

  /* create filter with a capacity of 1,000,000 bits */

  filter f(1000000);

  /* insert elements in the set */

  f.insert("hello");
  f.insert("Boost");

  /* elements inserted are always correctly checked as such */

  assert(f.may_contain("hello") == true);

  /* Elements not inserted may incorrectly be identified as such with a
   * false probability rate (FPR) which is a function of the array capacity,
   * the number of bits set per element and generally how the
   * boost::bloom::filter was configured.
   */

  if(f.may_contain("bye")) { /* likely false */
    std::cout << "false positive\n";
  }
  else {
    std::cout << "everything worked as expected\n";
  }
}
