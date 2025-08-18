
module boost.regex;

namespace boost::re_detail_600 {

   bool factory_match(perl_matcher<const wchar_t*, match_results<const wchar_t*>::allocator_type, wregex::traits_type>& m)
   {
      return m.match();
   }

}
