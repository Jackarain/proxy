#include <boost/core/lightweight_test.hpp>

#include <boost/regex.hpp>
#include <cstddef>
#include <vector>

template<std::size_t N0, std::size_t N = N0 - 1>
void tester( char const (&str)[ N0 ] )
{
   std::vector<char> s(N, '\0');
   std::memcpy(s.data(), str, N);
   boost::regex rx(s.begin(), s.end());

   std::vector<std::string> wheres;
   wheres.push_back(std::string(15, 'H'));
   wheres.push_back("");
   wheres.push_back("                         ");

   // Perl-style matching
   for (auto const& where : wheres) {
      boost::match_results<std::string::const_iterator> what;
      bool match = boost::regex_match(where, what, rx, boost::match_default | boost::match_partial | boost::match_any | boost::match_perl);
      (void) match;
   }

   // POSIX-style matching
   for (auto const& where : wheres) {
      try {
         boost::match_results<std::string::const_iterator> what;
         bool match = boost::regex_match(where, what, rx, boost::match_default | boost::match_partial | boost::match_any | boost::match_posix);
         (void) match;
      } catch(...) {}
   }

}

int main()
{
   // test strings derived from fuzzing
   // we keep a simple human-readable version
   char const str1[] = "(Y(*COMMIT)|\\K\\D|.)+";
   char const str2[] = "(Y(*COMMIT){||\\K\\D|||||||||\\K|||ss|||||.|\232*(?(50)\027\0204657|H)\020}\031\000.* 6.'?-i)+[L??.\000\000\000\004\000\000\000\000?..<[\000\024R]*+";
   char const str3[] = "(Y(*COMMIT)\xFF\x80|\\L\\K||||||||||.|||||\x84|||||\x00\x00\x10||||||.*  .'?-i)[L??...-i)[L??...[\x00\x14R]*+";
   char const str4[] = "(Y(*COMMIT)\x96||.*  .*  .\\K|||\x9F||||\x9C|.|||||\x84\x99|||\x01\x00\x00\x00|||'?-i#PL\x00\x01.\x86??OMMIT)?...[\x00\x14R]*+";

   tester(str1);
   tester(str2);
   tester(str3);
   tester(str4);

   // prove that we catch certain impossible scenarios

   {
      char const* str = "abcd";
      boost::regex rx(str);
      boost::match_results<std::string::const_iterator> what;
      std::string where(15, 'H');
      BOOST_TEST_THROWS(boost::regex_match(where, rx, boost::match_posix | boost::match_perl), std::logic_error);
   }

   {
      char const* str = "ab(*COMMIT)cd";
      boost::regex rx(str);
      boost::match_results<std::string::const_iterator> what;
      std::string where(15, 'H');
      BOOST_TEST_THROWS(boost::regex_match(where, rx, boost::match_posix), std::logic_error);
   }

   return boost::report_errors();
}

