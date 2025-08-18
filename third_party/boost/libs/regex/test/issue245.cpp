#include <boost/regex.hpp>

#include <vector>
#include <string>

#include "test_macros.hpp"


int main()
{
   // invalid because \k-- is an unterminated token
   {
      char const strdata[] = "\\k--00000000000000000000000000000000000000000000000000000000009223372036854775807\xff\xff\xff\xff\xff\xff\xff\xef""99999999999999999999999999999999999]999999999999999\x90";
      std::string regex_string(strdata, strdata + sizeof(strdata) - 1);
      BOOST_TEST_THROWS((boost::regex(regex_string)), boost::regex_error);
   }
   {
      char const strdata[] = "\\k-00000000000000000000000000000000000000000000000000000000009223372036854775807\xff\xff\xff\xff\xff\xff\xff\xef""99999999999999999999999999999999999]999999999999999\x90";
      std::string regex_string(strdata, strdata + sizeof(strdata) - 1);
      BOOST_TEST_THROWS((boost::regex(regex_string)), boost::regex_error);
   }
   {
      char const strdata[] = "\\k00000000000000000000000000000000000000000000000000000000009223372036854775807\xff\xff\xff\xff\xff\xff\xff\xef""99999999999999999999999999999999999]999999999999999\x90";
      std::string regex_string(strdata, strdata + sizeof(strdata) - 1);
      BOOST_TEST_THROWS((boost::regex(regex_string)), boost::regex_error);
   }
   {
      char const strdata[] = "a(b*)c\\k{--1}d";
      std::string regex_string(strdata, strdata + sizeof(strdata) - 1);
      BOOST_TEST_THROWS((boost::regex(regex_string)), boost::regex_error);
   }
   {
      char const strdata[] = "a(b*)c\\k-{-1}d";
      std::string regex_string(strdata, strdata + sizeof(strdata) - 1);
      BOOST_TEST_THROWS((boost::regex(regex_string)), boost::regex_error);
   }
   {
      char const strdata[] = "\\k{--00000000000000000000000000000000000000000000000000000000009223372036854775807}\xff\xff\xff\xff\xff\xff\xff\xef""99999999999999999999999999999999999]999999999999999\x90";
      std::string regex_string(strdata, strdata + sizeof(strdata) - 1);
      BOOST_TEST_THROWS((boost::regex(regex_string)), boost::regex_error);
   }
   {
      char const strdata[] = "\\k{-00000000000000000000000000000000000000000000000000000000009223372036854775807}\xff\xff\xff\xff\xff\xff\xff\xef""99999999999999999999999999999999999]999999999999999\x90";
      std::string regex_string(strdata, strdata + sizeof(strdata) - 1);
      BOOST_TEST_THROWS((boost::regex(regex_string)), boost::regex_error);
   }
   {
      char const strdata[] = "\\k{00000000000000000000000000000000000000000000000000000000009223372036854775807}\xff\xff\xff\xff\xff\xff\xff\xef""99999999999999999999999999999999999]999999999999999\x90";
      std::string regex_string(strdata, strdata + sizeof(strdata) - 1);
      BOOST_TEST_THROWS((boost::regex(regex_string)), boost::regex_error);
   }

   return boost::report_errors();
}
