#include <boost/regex.hpp>

#include <string>

#include "test_macros.hpp"

int main()
{
   char const strdata1[] = "\x00t\x03.z%(?x:]*+\x0c#\\x0c\x0c\x0c+\x0c#\\x0c\x0c\x0c\x11\x0c\x0c\xff\xff\xfd*\xff\xff\xff\xff\xff\xff\xff\xff|\xff\xff\xfd*\xff\xff)*\x01\x03\x00\x00\x00\x03\xff\xff\xff\x00\x00\xff\xff\xff";
   char const strdata2[] = "(?x:]*+#comment\n+)*";
   
   std::string str1(strdata1, strdata1 + sizeof(strdata1) - 1);
   std::string str2(strdata2, strdata2 + sizeof(strdata2) - 1);

   boost::match_results<std::string::const_iterator> what;
   
   BOOST_TEST_THROWS((boost::regex(str1)), boost::regex_error);
   BOOST_TEST_THROWS((boost::regex(str2)), boost::regex_error);

   return boost::report_errors();
}
