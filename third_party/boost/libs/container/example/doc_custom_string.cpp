//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2013-2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
//[doc_custom_string
#include <boost/container/string.hpp>

//Make sure assertions are active
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

int main ()
{
   using namespace boost::container;

//--------------------------
//    Option "stored_size"
//--------------------------

   //This option specifies that a string that will use "unsigned char" as
   //the type to store capacity or size internally.
   typedef string_options< stored_size<unsigned char> >::type size_option_t;

   //Size-optimized string is smaller than the default one.
   typedef basic_string<char, std::char_traits<char>, void, size_option_t > size_optimized_string_t;
   assert(( sizeof(size_optimized_string_t) < sizeof(basic_string<char>) ));

   //The internal small string optimization buffer is also smaller
   assert(( size_optimized_string_t().capacity() < basic_string<char>().capacity() ));

   //Requesting capacity for more elements than representable by half of the 
   //"unsigned char" max (due to the need to a bit to indicate short/long representation)
   //minus 1 (due to the terminating null) is an error in the size optimized string.
   bool exception_thrown = false;
   /*<-*/ 
   #ifndef BOOST_NO_EXCEPTIONS
   BOOST_CONTAINER_TRY{ size_optimized_string_t v(127, 'a');} BOOST_CONTAINER_CATCH(...) { exception_thrown = true; } BOOST_CONTAINER_CATCH_END
   #else
   exception_thrown = true;
   #endif   //BOOST_NO_EXCEPTIONS
   /*->*/
   //=try       { size_optimized_string_t v(127, 'a'); }
   //=catch(...){ exception_thrown = true;             }

   assert(exception_thrown == true);

//--------------------------
//    Option "inline_chars"
//--------------------------

   //This option specifies the capacity of the internally stored buffer
   //The maximum value due to internal data organization is 127 chars
   typedef string_options< inline_chars<100> >::type inline_chars_option_t;
   typedef basic_string<char, std::char_traits<char>, void, inline_chars_option_t > inline100_string_t;

   //The size of the object will grow accordingly
   assert(( sizeof(inline100_string_t) > sizeof(basic_string<char>) ));
   assert(( sizeof(inline100_string_t) > 100 ));

   //Internal capacity will be at least the specified one
   assert((inline100_string_t().capacity() >= 100));

//--------------------------
//    Option "growth_factor"
//--------------------------

   //This option specifies that a string will increase its capacity 50%
   //each time the previous capacity was exhausted.
   typedef string_options< growth_factor<growth_factor_50> >::type growth_50_option_t;

   //Fill the string until full capacity is reached
   basic_string<char, std::char_traits<char>, void, growth_50_option_t > growth_50_string(5, 0);
   const std::size_t old_cap = growth_50_string.capacity();
   growth_50_string.resize(old_cap);

   //Now insert an additional item and check the new buffer is aproximately
   //50% bigger (rounding due to the null terminator might round down)
   growth_50_string.push_back(1);
   assert(growth_50_string.capacity() == old_cap*3/2);

   return 0;
}
//]
