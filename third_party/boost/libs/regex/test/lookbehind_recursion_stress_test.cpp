/*
 *
 * Copyright (c) 2024
 * John Maddock
 *
 * Use, modification and distribution are subject to the
 * Boost Software License, Version 1.0. (See accompanying file
 * LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

#include <boost/regex.hpp>

int main()
{
   std::string s("(?<=(");
   s.append(1000, '|');
   s += "))";
   boost::regex rx(s);

   s = "(?<=(a";
   for (unsigned i = 0; i < 1000; ++i)
   {
      s += "|a";
   }
   s += "))";
   boost::regex rx2(s);
}
