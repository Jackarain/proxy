///////////////////////////////////////////////////////////////////////////////
//  Copyright 2019 - 2025 John Maddock.
//  Copyright 2025 Christopher Kormanyos.
//  Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <test.hpp>

#include <boost/multiprecision/cpp_dec_float.hpp>

#include <locale>
#include <string>

int main()
{
   std::string str { };

   // Test strings with a decimal point. Keep in the spirit of the
   // original test case. But don't simply try-catch out of it.

   try
   {
      using local_dec_float_type = boost::multiprecision::cpp_dec_float_50;

      // Test a string with a decimal point.
      local_dec_float_type d_first("1234.56");
      str = d_first.str();
      BOOST_CHECK(str == "1234.56");

      #if (defined(__GNUC__) && defined(__x86_64__) && !(defined(_WIN32) || defined(__MINGW32__)) && !defined(__APPLE__))

      // That preprocessor line is detailed, but it really essentially is
      // just trying to isolate Ubuntu runners on GHA for test and coverage.

      // Set a new local locale. All these compilers have different
      // locale names. In this particular case, we stay with the
      // original spirit of the test in issue167 at least on Ubuntu
      // GHA runners. We also wanto to pick up these lines of coverage.

      const char* p_str_loc = std::setlocale(LC_ALL, "C.UTF-8");

      std::cout << "A local locale was set: " << p_str_loc << std::endl;

      {
         std::stringstream strm { };

         strm << p_str_loc;

         // Check the new local locale.
         BOOST_CHECK(strm.str().find("UTF") != std::string::npos);
      }
      #endif

      local_dec_float_type d_other("123.789");
      str = d_other.str();

      BOOST_CHECK(str == "123.789");
   }
   catch(const std::runtime_error&) { }  // LCOV_EXCL_LINE No locale was found.

   BOOST_CHECK(str == "123.789");

   return boost::report_errors();
}
