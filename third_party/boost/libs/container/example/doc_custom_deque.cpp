//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2013-2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
//[doc_custom_deque
#include <boost/container/deque.hpp>

//Make sure assertions are active
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

int main ()
{
   using namespace boost::container;

   //This option specifies that a deque that will use "unsigned char" as
   //the type to store capacity or size internally.
   typedef deque_options< stored_size<unsigned char> >::type size_option_t;

   //Size-optimized deque is smaller than the default one.
   typedef deque<int, new_allocator<int>, size_option_t > size_optimized_deque_t;
   assert(( sizeof(size_optimized_deque_t) < sizeof(deque<int>) ));

   //Requesting capacity for more elements than representable by "unsigned char"
   //is an error in the size optimized deque.
   bool exception_thrown = false;
   /*<-*/ 
   #ifndef BOOST_NO_EXCEPTIONS
   BOOST_CONTAINER_TRY{ size_optimized_deque_t v(256); } BOOST_CONTAINER_CATCH(...){ exception_thrown = true; } BOOST_CONTAINER_CATCH_END
   #else
   exception_thrown = true;
   #endif   //BOOST_NO_EXCEPTIONS
   /*->*/
   //=try       { size_optimized_deque_t v(256); }
   //=catch(...){ exception_thrown = true;        }

   assert(exception_thrown == true);

   //This option specifies the desired block size for deque
   typedef deque_options< block_size<128u> >::type block_128_option_t;

   //This deque will allocate blocks of 128 elements
   typedef deque<int, void, block_128_option_t > block_128_deque_t;
   assert(block_128_deque_t::get_block_size() == 128u);

   //This option specifies the maximum block size for deque
   //in bytes
   typedef deque_options< block_bytes<1024u> >::type block_1024_bytes_option_t;

   //This deque will allocate blocks of 1024 bytes
   typedef deque<int, void, block_1024_bytes_option_t > block_1024_bytes_deque_t;
   assert(block_1024_bytes_deque_t::get_block_size() == 1024u/sizeof(int));

   return 0;
}
//]
