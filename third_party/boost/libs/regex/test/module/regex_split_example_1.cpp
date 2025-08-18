/*
 *
 * Copyright (c) 1998-2022
 * John Maddock
 *
 * Use, modification and distribution are subject to the 
 * Boost Software License, Version 1.0. (See accompanying file 
 * LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

 /*
  *   LOCATION:    see http://www.boost.org for most recent version.
  *   FILE         regex_split_example_1.cpp
  *   VERSION      see <boost/version.hpp>
  *   DESCRIPTION: regex_split example: split a string into tokens.
  */


#if (defined(__cpp_lib_modules) || (defined(_MSC_FULL_VER) && (_MSC_FULL_VER >= 193833135))) && !defined(TEST_HEADERS)
import std;
#elif defined(MSVC_EXPERIMENTAL_STD_MODULE) && !defined(TEST_HEADERS)
Simport std.core;
#else
#include <list>
#include <string>
#include <iostream>
#endif

#ifdef TEST_HEADERS
#include <boost/regex.hpp>
#else
import boost.regex;
#endif

unsigned tokenise(std::list<std::string>& l, std::string& s)
{
   return boost::regex_split(std::back_inserter(l), s);
}

using namespace std;

int main(int argc, const char*[])
{
   string s;
   list<string> l;
   do{
      if(argc == 1)
      {
         cout << "Enter text to split (or \"quit\" to exit): ";
         getline(cin, s);
         if(s == "quit") break;
      }
      else
         s = "This is a string of tokens";
      unsigned result = tokenise(l, s);
      cout << result << " tokens found" << endl;
      cout << "The remaining text is: \"" << s << "\"" << endl;
      while(l.size())
      {
         s = *(l.begin());
         l.pop_front();
         cout << s << endl;
      }
   }while(argc == 1);
   return 0;
}


