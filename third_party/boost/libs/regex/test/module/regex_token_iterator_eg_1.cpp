/*
 *
 * Copyright (c) 2003-2022
 * John Maddock
 *
 * Use, modification and distribution are subject to the 
 * Boost Software License, Version 1.0. (See accompanying file 
 * LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

 /*
  *   LOCATION:    see http://www.boost.org for most recent version.
  *   FILE         regex_token_iterator_example_1.cpp
  *   VERSION      see <boost/version.hpp>
  *   DESCRIPTION: regex_token_iterator example: split a string into tokens.
  */


#if (defined(__cpp_lib_modules) || (defined(_MSC_FULL_VER) && (_MSC_FULL_VER >= 193833135))) && !defined(TEST_HEADERS)
import std;
#elif defined(MSVC_EXPERIMENTAL_STD_MODULE) && !defined(TEST_HEADERS)
import std.core;
#else
#include <iostream>
#include <string>
#endif

#ifdef TEST_HEADERS
#include <boost/regex.hpp>
#else
import boost.regex;
#endif

using namespace std;

int main(int argc, const char*[])
{
   string s;
   do{
      if(argc == 1)
      {
         cout << "Enter text to split (or \"quit\" to exit): ";
         getline(cin, s);
         if(s == "quit") break;
      }
      else
         s = "This is a string of tokens";

      boost::regex re("\\s+");
      boost::sregex_token_iterator i(s.begin(), s.end(), re, -1);
      boost::sregex_token_iterator j;

      unsigned count = 0;
      while(i != j)
      {
         cout << *i++ << endl;
         count++;
      }
      cout << "There were " << count << " tokens found." << endl;

   }while(argc == 1);
   return 0;
}


