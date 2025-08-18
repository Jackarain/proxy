
module boost.regex;

namespace boost::re_detail_600 {

   bool factory_find(perl_matcher<const char*, match_results<const char*>::allocator_type, regex::traits_type>& m)
   {
      return m.find();
   }

}
