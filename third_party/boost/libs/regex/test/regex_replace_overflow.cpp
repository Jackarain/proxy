#include <boost/regex.hpp>

#include <iostream>
#include <iterator>
#include <sstream>
#include <string>

#include <boost/core/lightweight_test.hpp>

int main() {
   std::string format_string = "$2$2147483647";
   boost::regex e2("(<)|(>)|(&)|\\r");

   std::string in = 
      "#include <iostream>"
      ""
      "int main() { std::cout << \"Hello, world!\\n\"; }";

   std::ostringstream t( std::ios::out | std::ios::binary );
   std::ostream_iterator<char, char> oi( t );

   boost::regex_replace(oi, in.begin(), in.end(), e2, format_string,
                        boost::match_default | boost::format_all);

   std::string s(t.str());

   BOOST_TEST(!s.empty());
   return boost::report_errors();
}
