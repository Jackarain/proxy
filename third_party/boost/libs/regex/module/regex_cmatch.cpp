
module boost.regex;

namespace boost::re_detail_600 {

   bool factory_match(perl_matcher<const char*, match_results<const char*>::allocator_type, regex::traits_type>& m)
   {
      return m.match();
   }

}
