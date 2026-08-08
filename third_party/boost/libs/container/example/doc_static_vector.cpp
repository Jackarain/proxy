//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2024-2024. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
//[doc_static_vector
#include <boost/container/static_vector.hpp>

#include <exception>

//Make sure assertions are active
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

int main ()
{
   using namespace boost::container;

   //A static_vector<int> that can hold at most 5 elements. All the storage
   //lives inside the object itself, so creating it performs no dynamic
   //allocation and the capacity is fixed at compile time.
   static_vector<int, 5> sv;

   assert(sv.empty());
   assert(sv.capacity() == 5);   //fixed capacity, known at compile time
   assert(sv.max_size() == 5);

   //Elements are inserted just like with a vector (constant time at the end).
   //Unlike std::array, elements are constructed only as they are inserted.
   for(int i = 0; i < 5; ++i)
      sv.push_back(i);

   assert(sv.size() == 5);
   assert(sv.size() == sv.capacity());   //the container is now full

   //Contiguous storage and random access, just like vector/array.
   assert(sv[0] == 0 && sv[4] == 4);
   assert(sv.data()[2] == 2);

   #ifndef BOOST_NO_EXCEPTIONS
   //By default (throw_on_overflow<true>) inserting beyond the fixed capacity
   //throws instead of allocating more memory.
   bool overflowed = false;
   try {
      sv.push_back(5);   //capacity already reached
   }
   catch(const std::exception &) {
      overflowed = true;
   }
   assert(overflowed);
   #endif

   //Removal at the end is constant time and never reallocates.
   sv.pop_back();
   assert(sv.size() == 4);

   return 0;
}
//]
